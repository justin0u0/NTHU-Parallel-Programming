#ifndef _JOB_TRACKER_H_
#define _JOB_TRACKER_H_

#include <chrono>
#include <list>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#include "logger.h"
#include "task_tracker.h"
#include "types.h"

class JobTracker {
private:
  Config* config;

  // list of task id that has not been dispatch
  std::list<int> tasks;

  // nodes that request for mapper task
  std::queue<int> mapperTaskRequests;
  // nodes that request for reducer task
  std::queue<int> reducerTaskRequests;

  int inflightMapperTasks;
  int inflightReducerTasks;
  int workingTaskTrackers;
  int totalMapperKeys;
  int totalShuffleStarts;
  int totalShuffleEnds;

  std::chrono::time_point<std::chrono::system_clock> start;
  std::chrono::time_point<std::chrono::system_clock> shuffleStart;

  pthread_t server_t;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  Logger* logger;

  static void* serve(void* arg) {
    JobTracker* jt = (JobTracker*)arg;

    Message resp;
    MPI_Status status;

    while (jt->workingTaskTrackers != 0) {
      MPI_Recv(resp.raw, MESSAGE_SIZE, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

      pthread_mutex_lock(&jt->mutex);
      switch (resp.data.type) {
        case MessageType::MAP:
          jt->mapperTaskRequests.push(status.MPI_SOURCE);
          pthread_cond_signal(&jt->cond);
          break;
        case MessageType::SHUFFLE:
          jt->totalMapperKeys += resp.data.data;
          ++jt->totalShuffleStarts;
          if (jt->totalShuffleStarts == jt->config->numMappers) {
            jt->shuffleStart = std::chrono::system_clock::now();
            jt->logger->log() << "Start_Shuffle," << jt->totalMapperKeys << std::endl;
          }
          break;
        case MessageType::REDUCE:
          jt->reducerTaskRequests.push(status.MPI_SOURCE);
          pthread_cond_signal(&jt->cond);
          break;
        case MessageType::MAP_DONE:
          --jt->inflightMapperTasks;
          jt->logger->log() << "Complete_MapTask," << resp.data.taskId << ',' << resp.data.data << std::endl;
          break;
        case MessageType::SHUFFLE_DONE:
          ++jt->totalShuffleEnds;
          if (jt->totalShuffleEnds == jt->config->numReducers) {
            auto elapsed = std::chrono::system_clock::now() - jt->shuffleStart;
            jt->logger->log() << "Finish_Shuffle," << std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() << std::endl;
          }
          break;
        case MessageType::REDUCE_DONE:
          --jt->inflightReducerTasks;
          jt->logger->log() << "Complete_ReduceTask," << resp.data.taskId << ',' << resp.data.data << std::endl;
          break;
        case MessageType::TERMINATE:
          --jt->workingTaskTrackers;
          break;
      }
      pthread_mutex_unlock(&jt->mutex);
    }

    return nullptr;
  }
public:
  JobTracker(Config* config) : config(config) {
    pthread_mutex_init(&mutex, 0);
    pthread_cond_init(&cond, 0);

    // load taskId (chuckId), taskId is between 1 ~ numMappers
    for (int i = 1; i <= config->numMappers; ++i) {
      tasks.emplace_back(i);
    }

    inflightMapperTasks = 0;
    inflightReducerTasks = 0;
    workingTaskTrackers = config->nodes - 1;
    totalMapperKeys = 0;
    totalShuffleStarts = 0;
    totalShuffleEnds = 0;

    start = std::chrono::system_clock::now();

    logger = new Logger(config->logFilename(), Logger::Level::INFO);
  }

  ~JobTracker() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    delete logger;
  }

  void run() {
    logger->log() << "Start_Job," << config->jobName << ',' << config->nodes << ','
      << config->cpus << ',' << config->numReducers << ',' << config->delay << ','
      << config->inputFilename << ',' << config->chunkSize << ','
      << config->localityConfigFilename << ',' << config->outputDir << std::endl;

    pthread_create(&server_t, 0, JobTracker::serve, (void*)this);

    for (int i = 1; i <= config->numMappers; ++i) {
      // generate mapper tasks

      pthread_mutex_lock(&mutex);

      ++inflightMapperTasks;

      while (mapperTaskRequests.empty()) {
        pthread_cond_wait(&cond, &mutex);
      }

      int nodeId = mapperTaskRequests.front();
      mapperTaskRequests.pop();

      pthread_mutex_unlock(&mutex);

      // find data with locality
      std::list<int>::iterator it;
      for (it = tasks.begin(); it != tasks.end(); ++it) {
        if (config->localityConfig[*it] == nodeId) {
          break;
        }
      }

      // if no matching data with the node, pick the first one
      if (it == tasks.end()) {
        it = tasks.begin();
      }

      int taskId = *it;
      tasks.erase(it);

      // send event to task tracker
      Message req = Message{.data = {
        .type = MessageType::MAP,
        .id = i,
        .taskId = taskId
      }};

      logger->log() << "Dispatch_MapTask," << taskId << ',' << i << std::endl;
      // logger->log(Logger::Level::DEBUG) << "With_Locality," << (config->localityConfig[taskId] == nodeId ? "true" : "false") << std::endl;
      MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, nodeId, 0, MPI_COMM_WORLD);
    }

    while (inflightMapperTasks > 0) {
      usleep(1000);
    }

    for (int i = 0; i < config->numReducers; ++i) {
      // generate reducer tasks
      pthread_mutex_lock(&mutex);

      ++inflightReducerTasks;

      while (reducerTaskRequests.empty()) {
        pthread_cond_wait(&cond, &mutex);
      }

      int nodeId = reducerTaskRequests.front();
      reducerTaskRequests.pop();

      pthread_mutex_unlock(&mutex);

      // send event to task tracker

      int taskId = i;
      
      Message req = Message{.data = {
        .type = MessageType::REDUCE,
        .id = i + 1,
        .taskId = taskId
      }};

      logger->log() << "Dispatch_ReduceTask," << taskId << ',' << i + 1 << std::endl;
      MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, nodeId, 0, MPI_COMM_WORLD);
    }

    while (inflightReducerTasks > 0) {
      usleep(1000);
    }

    Message req = Message{.data = {
      .type = MessageType::TERMINATE
    }};
    for (int i = 1; i < config->nodes; ++i) {
      MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    pthread_join(server_t, 0);

    auto elapsed = std::chrono::system_clock::now() - start;
    logger->log() << "Finish_Job," << std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() << std::endl;
  }
};

#endif
