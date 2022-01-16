#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <pthread.h>
#include <queue>

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
  pthread_cond_t addCond;
  pthread_cond_t removeCond;

  std::queue<ThreadPoolTask*> tasks;
  const size_t bufferSize = 1;

  bool terminating;

  ThreadPoolTask* removeTask() {
    pthread_mutex_lock(&mutex);

    while (tasks.empty() && !terminating) {
      // sleep until addTask notify
      pthread_cond_wait(&removeCond, &mutex);
    }

    if (terminating) {
      pthread_mutex_unlock(&mutex);
      return nullptr;
    }

    ThreadPoolTask* task = tasks.front();
    tasks.pop();
    pthread_cond_signal(&addCond);

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
    pthread_cond_init(&addCond, 0);
    pthread_cond_init(&removeCond, 0);
  }

  ~ThreadPool() {
    pthread_cond_destroy(&addCond);
    pthread_cond_destroy(&removeCond);
    pthread_mutex_destroy(&mutex);

    while (!tasks.empty()) {
      ThreadPoolTask* task = tasks.front();
      tasks.pop();

      delete task;
    }

    delete[] threads;
  }

  void addTask(ThreadPoolTask* task) {
    pthread_mutex_lock(&mutex);

    while (tasks.size() >= bufferSize && !terminating) {
      // sleep until removeTask notify
      pthread_cond_wait(&addCond, &mutex);
    }

    if (terminating) {
      pthread_mutex_unlock(&mutex);
      return;
    }

    tasks.push(task);
    pthread_cond_signal(&removeCond);

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
    pthread_mutex_lock(&mutex);

    terminating = true;
    pthread_cond_broadcast(&addCond);
    pthread_cond_broadcast(&removeCond);

    pthread_mutex_unlock(&mutex);
  }
};

#endif // _THREAD_POOL_H_
