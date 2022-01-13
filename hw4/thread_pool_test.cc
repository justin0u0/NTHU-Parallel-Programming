#include <stdio.h>
#include <unistd.h>

#include "thread_pool.h"

void* sleepPrint(void* arg) {
	int value = *(int*)arg;

	sleep(2);

	printf("%d\n", value);

	return nullptr;
}

int main() {
	ThreadPool* pool = new ThreadPool(4);

	pool->start();

	int* arr = new int[100000];
	for (int i = 0; i < 100000; ++i) {
		arr[i] = i;
		pool->addTask(new ThreadPoolTask(&sleepPrint, (void*)&arr[i]));
	}

	sleep(60);

	pool->terminate();
	printf("terminating\n");
	pool->join();
	printf("done.\n");

	delete[] arr;
}
