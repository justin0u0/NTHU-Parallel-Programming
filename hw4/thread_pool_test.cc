#include <stdio.h>
#include <unistd.h>

#include "thread_pool.h"

#define NUM_TASKS 10

void* sleepPrint(void* arg) {
	int value = *(int*)arg;

	sleep(2);

	printf("%d\n", value);

	return nullptr;
}

int main() {
	ThreadPool* pool = new ThreadPool(4);

	pool->start();

	int* arr = new int[NUM_TASKS];
	for (int i = 0; i < NUM_TASKS; ++i) {
		arr[i] = i;
		pool->addTask(new ThreadPoolTask(&sleepPrint, (void*)&arr[i]));
	}

	sleep(10);

	pool->terminate();
	pool->join();

	delete[] arr;
	
	delete pool;

	return 0;
}
