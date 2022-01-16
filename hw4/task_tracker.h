#ifndef _TASK_TRACKER_H_
#define _TASK_TRACKER_H_

#include <mpi.h>
#include <mutex>
#include <pthread.h>
#include <unistd.h>

#include "thread_pool.h"
#include "mapper.h"
#include "reducer.h"
#include "types.h"

#define JOB_TRACKER_NODE 0

class TaskTracker {
private:
  int nodeId;

  Config* config;

  ThreadPool* mapperPool;
  ThreadPool* reducerPool;

  bool terminating;

  void requestMapperTask() {
    Message req = {.data = {.type = MessageType::MAP}};

    MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
  }

  void requestReducerTask() {
    Message req = {.data = {.type = MessageType::REDUCE}};

    MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
  }

  void serve() {
    requestMapperTask();
    requestReducerTask();

    Message resp;

    while (!terminating) {
      MPI_Recv(resp.raw, MESSAGE_SIZE, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      switch (resp.data.type) {
        case MessageType::MAP: {
          // dispatch mapper task

          Mapper* mapper = new Mapper(resp.data.id, resp.data.taskId, nodeId, config, &TaskTracker::callback);
          mapperPool->addTask(new ThreadPoolTask(&Mapper::run, mapper));

          usleep(100000);
          requestMapperTask();

          break;
        }
        case MessageType::REDUCE: {
          // dispatch reducer task

          Reducer* reducer = new Reducer(resp.data.id, resp.data.taskId, config, &TaskTracker::callback);
          reducerPool->addTask(new ThreadPoolTask(&Reducer::run, reducer));

          usleep(100000);
          requestReducerTask();

          break;
        }
        case MessageType::TERMINATE: {
          terminating = true;
          break;
        }
      }
    }
  }
public:
  TaskTracker(int nodeId, Config* config) : nodeId(nodeId), config(config) {
    mapperPool = new ThreadPool(config->cpus - 1);
    reducerPool = new ThreadPool(1);

    terminating = false;
  }

  ~TaskTracker() {
    delete mapperPool;
    delete reducerPool;
  }

  void run() {
    mapperPool->start();
    reducerPool->start();

    serve();

    mapperPool->terminate();
    reducerPool->terminate();

    mapperPool->join();
    reducerPool->join();

    Message req = {.data = {.type = MessageType::TERMINATE}};
    MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
  }

  static void callback(MessageType type, int id, int taskId, int data) {
    static std::mutex mutex;

    // make callback atomic function
    std::lock_guard<std::mutex> lock(mutex);

    Message req = Message{.data = {
      .type = type,
      .id = id,
      .taskId = taskId,
      .data = data
    }};

    MPI_Send(req.raw, MESSAGE_SIZE, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
  }
};

#endif // _TASK_TRACKER_H_
