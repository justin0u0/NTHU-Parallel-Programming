#ifndef _JOB_TRACKER_H_
#define _JOB_TRACKER_H_

#include <mpi.h>
#include <pthread.h>
#include <queue>
#include <unistd.h>

#include "task_tracker.h"
#include "word_count_types.h"

class JobTracker {
private:
	WordCountConfig* config;

	// nodes that request for mapper task
	std::queue<int> mapperTaskRequests;
	// nodes that request for reducer task
	std::queue<int> reducerTaskRequests;

	int inflightMapperTasks;
	int inflightReducerTasks;
	int workingTaskTrackers;

	pthread_t server_t;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	static void* serve(void* arg) {
		JobTracker* jobTracker = (JobTracker*)arg;

		PayloadMessage resp;
		MPI_Status status;

		while (jobTracker->workingTaskTrackers != 0) {
			MPI_Recv(resp.raw, 3, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

			pthread_mutex_lock(&jobTracker->mutex);
			switch (resp.data.type) {
				case TaskType::MAP:
					jobTracker->mapperTaskRequests.push(status.MPI_SOURCE);
					pthread_cond_signal(&jobTracker->cond);
					break;
				case TaskType::REDUCE:
					jobTracker->reducerTaskRequests.push(status.MPI_SOURCE);
					pthread_cond_signal(&jobTracker->cond);
					break;
				case TaskType::MAP_DONE:
					--jobTracker->inflightMapperTasks;
					break;
				case TaskType::REDUCE_DONE:
					--jobTracker->inflightReducerTasks;
					break;
				case TaskType::TERMINATE:
					--jobTracker->workingTaskTrackers;
					break;
			}
			pthread_mutex_unlock(&jobTracker->mutex);
		}

		return nullptr;
	}
public:
	JobTracker(WordCountConfig* config) : config(config) {
		pthread_mutex_init(&mutex, 0);
		pthread_cond_init(&cond, 0);

		inflightMapperTasks = 0;
		inflightReducerTasks = 0;

		workingTaskTrackers = config->nodes - 1;
	}

	~JobTracker() {
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
	}

	void run() {
		pthread_create(&server_t, 0, JobTracker::serve, (void*)this);

		printf("numMappers: %d, numReducers: %d\n", config->numMappers, config->numReducers);

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

			// send event to task tracker

			// TODO: locality aware
			int taskId = i;

			PayloadMessage req = PayloadMessage{.data = {
				.id = i,
				.taskId = taskId,
				.type = TaskType::MAP
			}};

			MPI_Send(req.raw, 3, MPI_INT, nodeId, 0, MPI_COMM_WORLD);
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
			
			PayloadMessage req = PayloadMessage{.data = {
				.id = i + 1,
				.taskId = taskId,
				.type = TaskType::REDUCE
			}};

			MPI_Send(req.raw, 3, MPI_INT, nodeId, 0, MPI_COMM_WORLD);
		}

		while (inflightReducerTasks > 0) {
			usleep(1000);
		}

		PayloadMessage req = PayloadMessage{.data = {
			.type = TaskType::TERMINATE
		}};
		for (int i = 1; i < config->nodes; ++i) {
			MPI_Send(req.raw, 3, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		pthread_join(server_t, 0);
	}
};

#endif
