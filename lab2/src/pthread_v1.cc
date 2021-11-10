#include <stdio.h>
#include <math.h>
#include <sched.h>
#include <assert.h>
#include <pthread.h>

unsigned long long radius;
unsigned long long k;
unsigned long long rr;
unsigned long long l;
unsigned long long *partial_sum;

int ncpus;

void* calc(void* thread_id) {
	int tid = *(int*)thread_id;

	unsigned long long from = tid * (l / ncpus);
	unsigned long long to = (tid == ncpus - 1) ? l : (tid + 1) * (l / ncpus);
	unsigned long long prev = rr - from * from;
	unsigned long long pixels = ceil(sqrtl(prev));
	for (unsigned long long i = from; i < to - 1; ++i) {
		prev -= (i << 1) + 1;
		pixels += ceil(sqrtl(prev));
	}
	partial_sum[tid] = (pixels + k - ((from + to - 1) * (to - from) >> 1)) % k;
	pthread_exit(nullptr);
}

int main(int argc, char** argv) {
	assert(argc == 3);

	radius = atoll(argv[1]);
	k = atoll(argv[2]);
	rr = radius * radius;
	l = ceil(radius / sqrt(2));

	cpu_set_t cpuset;
	sched_getaffinity(0, sizeof(cpuset), &cpuset);
	ncpus = CPU_COUNT(&cpuset);
	// printf("ncpus: %d\n", ncpus);

	partial_sum = new unsigned long long[ncpus];
	pthread_t threads[ncpus];
	int* ids = new int[ncpus];

	for (int i = 0; i < ncpus; i++) {
		ids[i] = i;
		pthread_create(&threads[i], nullptr, calc, (void*)&ids[i]);
	}
	for (int i = 0; i < ncpus; i++)
		pthread_join(threads[i], nullptr);

	unsigned long long answer = 0;
	for (int i = 0; i < ncpus; i++)
		answer += partial_sum[i];
	printf("%llu\n", (4 * (2 * answer + k - l)) % k);
	return 0;
}
