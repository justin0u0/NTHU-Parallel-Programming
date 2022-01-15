#ifndef _TASK_TRACKER_H_
#define _TASK_TRACKER_H_

#include <mpi.h>
#include <pthread.h>

#include "thread_pool.h"
#include "word_count_mapper.h"
#include "word_count_reducer.h"
#include "word_count_types.h"

#define JOB_TRACKER_NODE 0

enum class TaskType : int {
	MAP,
	REDUCE,
	MAP_DONE,
	REDUCE_DONE,
	TERMINATE
};

// the payload message send between task tracker & job tracker for convinience
union PayloadMessage {
	struct {
		// mapper/reducer ID
		int id;
		
		// task ID
		// - the chuck ID for the mapper
		// - the partition ID for the reducer
		int taskId;

		// operation type
		TaskType type;
	} data;

	int raw[3];
};

class TaskTracker {
private:
	int nodeId;

	WordCountConfig* config;

	ThreadPool* mapperPool;
	ThreadPool* reducerPool;

	bool terminating;

	void requestMapperTask() {
		PayloadMessage req = {.data = {.type = TaskType::MAP}};

		printf("[TaskTracker::requestMapperTask]: request for a mapper task\n");
		MPI_Send(req.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
	}

	void requestReducerTask() {
		PayloadMessage req = {.data = {.type = TaskType::REDUCE}};

		printf("[TaskTracker::requestReducerTask]: request for a reducer task\n");
		MPI_Send(req.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
	}

	void serve() {
		requestMapperTask();
		requestReducerTask();

		PayloadMessage resp;

		while (!terminating) {
			MPI_Recv(resp.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			switch (resp.data.type) {
				case TaskType::MAP: {
					// dispatch mapper task

					WordCountMapper* mapper = new WordCountMapper(resp.data.id, resp.data.taskId, config, &TaskTracker::mapperCallback);
					mapperPool->addTask(new ThreadPoolTask(&WordCountMapper::run, mapper));

					requestMapperTask();

					break;
				}
				case TaskType::REDUCE: {
					// dispatch reducer task

					WordCountReducer* reducer = new WordCountReducer(resp.data.id, resp.data.taskId, config, &TaskTracker::reducerCallback);
					reducerPool->addTask(new ThreadPoolTask(&WordCountReducer::run, reducer));

					requestReducerTask();

					break;
				}
				case TaskType::TERMINATE: {
					printf("[TaskTracker::serve] terminating\n");
					terminating = true;
					break;
				}
			}
		}
	}
public:
	TaskTracker(int nodeId, WordCountConfig* config) : nodeId(nodeId), config(config) {
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

		printf("[TaskTracker::run] terminating mapperPool\n");
		mapperPool->terminate();
		mapperPool->join();

		printf("[TaskTracker::run] terminating reducerPool\n");
		// reducerPool->terminate();
		// reducerPool->join();

		PayloadMessage req = {.data = {.type = TaskType::TERMINATE}};
		MPI_Send(req.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
	}

	static void mapperCallback(int id, int taskId) {
		printf("callback from mapper %d %d\n", id, taskId);

		PayloadMessage req = PayloadMessage{.data = {
			.id = id,
			.taskId = taskId,
			.type = TaskType::MAP_DONE
		}};

		MPI_Send(req.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
	}

	static void reducerCallback(int id, int taskId) {
		printf("callback from reducer %d %d\n", id, taskId);

		PayloadMessage req = PayloadMessage{.data = {
			.id = id,
			.taskId = taskId,
			.type = TaskType::REDUCE_DONE
		}};

		MPI_Send(req.raw, 3, MPI_INT, JOB_TRACKER_NODE, 0, MPI_COMM_WORLD);
	}
};

#endif // _TASK_TRACKER_H_
