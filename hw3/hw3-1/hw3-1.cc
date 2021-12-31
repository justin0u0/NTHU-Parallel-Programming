#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <pthread.h>

const int INF = ((1 << 30) - 1);

/* global sharing variables */
int n, m;
int num_cpus;
int* d_alloc;
int** d;
pthread_barrier_t barrier;

void handle_input(const char* input_file) {
	FILE* file = fopen(input_file, "rb");
	fread(&n, sizeof(int), 1, file);
	fread(&m, sizeof(int), 1, file);

	d_alloc = (int*)malloc(n * n * sizeof(int));

	d = (int**)malloc(n * sizeof(int*));
	for (int i = 0; i < n; ++i) {
		d[i] = d_alloc + i * n;
		for (int j = 0; j < n; ++j) {
			if (i == j) {
				d[i][j] = 0;
			} else {
				d[i][j] = INF;
			}
		}
	}

	int edge[3];
	for (int i = 0; i < m; ++i) {
		fread(edge, sizeof(int), 3, file);
		d[edge[0]][edge[1]] = edge[2];
	}
	fclose(file);
}

void handle_output(const char* output_file) {
	FILE* file = fopen(output_file, "w");
	fwrite(d_alloc, sizeof(int), n * n, file);
	fclose(file);
}

void* floyd_warshall(void* arg) {
	int tid = *(int*)arg;

	int from = tid * (n / num_cpus) + std::min(tid, n % num_cpus);
	int to = from + n / num_cpus + (tid < n % num_cpus);

	for (int k = 0; k < n; ++k) {
		for (int i = from; i < to; ++i) {
			for (int j = 0; j < n; ++j) {
				d[i][j] = std::min(d[i][j], d[i][k] + d[k][j]);
			}
		}

		pthread_barrier_wait(&barrier);
	}

	return NULL;
}

int main(int argc, char** argv) {
	// initialize pthreads
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  num_cpus = CPU_COUNT(&cpu_set);
	pthread_barrier_init(&barrier, NULL, num_cpus);

	// handle input
	handle_input(argv[1]);

	// launch threads to calculate floyd warshall
	pthread_t* threads = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	int* thread_ids = (int*)malloc(num_cpus * sizeof(int));
	for (int i = 0; i < num_cpus; ++i) {
		thread_ids[i] = i;
		pthread_create(&threads[i], NULL, floyd_warshall, (void*)&thread_ids[i]);
	}

	for (int i = 0; i < num_cpus; ++i) {
		pthread_join(threads[i], NULL);
	}

	// handle output
	handle_output(argv[2]);
	
	delete[] d_alloc;

	return 0;
}
