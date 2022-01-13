#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <pthread.h>
#include <queue>
#include <stdio.h>

class ThreadPool;

class ThreadPoolTask {
	friend class ThreadPool;
public:
	ThreadPoolTask(void* (*f)(void*), void* arg) : f(f), arg(arg) {}
private:
	void* (*f)(void*);
	void* arg;
};

class ThreadPool {
private:
	int numThreads;

	pthread_t* threads;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	std::queue<ThreadPoolTask*> tasks;

	bool terminating;

	ThreadPoolTask* removeTask() {
		pthread_mutex_lock(&mutex);

		while (tasks.empty() && !terminating) {
			// sleep until addTask notify
			pthread_cond_wait(&cond, &mutex);
		}

		if (terminating) {
			return nullptr;
		}

		ThreadPoolTask* task = tasks.front();
		tasks.pop();

		pthread_mutex_unlock(&mutex);

		return task;
	}

	static void* run(void* arg) {
		ThreadPool* pool = (ThreadPool*)arg;

		while (!pool->terminating) {
			ThreadPoolTask* task = pool->removeTask();

			if (task != nullptr) {
				// do the task works
				(*(task->f))(task->arg);

				delete task;
			}
		}

		return nullptr;
	}
public:
	ThreadPool(int numThreads) : numThreads(numThreads) {
		threads = new pthread_t[numThreads];

		terminating = false;

		pthread_mutex_init(&mutex, 0);
		pthread_cond_init(&cond, 0);
	}

	~ThreadPool() {
		pthread_cond_destroy(&cond);
		pthread_mutex_destroy(&mutex);
		
		delete[] threads;
	}

	void addTask(ThreadPoolTask* task) {
		pthread_mutex_lock(&mutex);

		tasks.push(task);
		pthread_cond_signal(&cond);

		pthread_mutex_unlock(&mutex);
	}

	void start() {
		for (int i = 0; i < numThreads; ++i) {
			pthread_create(&threads[i], 0, ThreadPool::run, (void*)this);
		}
	}

	void join() {
		for (int i = 0; i < numThreads; ++i) {
			pthread_join(threads[i], 0);
		}
	}

	void terminate() {
		terminating = true;

		pthread_cond_broadcast(&cond);	
	}
};

#endif // _THREAD_POOL_H
