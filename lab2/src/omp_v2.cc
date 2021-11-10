#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
	assert(argc == 3);

	unsigned long long radius = atoll(argv[1]);
	unsigned long long k = atoll(argv[2]);
	unsigned long long rr = radius * radius;
	unsigned long long l = ceil(radius / sqrt(2));

	int num_threads;
	unsigned long long* partial_sum;

	#pragma omp parallel
	{
		#pragma omp single
		{
			num_threads = omp_get_num_threads();
			partial_sum = new unsigned long long[num_threads];
		}

		int tid = omp_get_thread_num();

		unsigned long long from = tid * (l / num_threads);
		unsigned long long to = (tid == num_threads - 1) ? l : (tid + 1) * (l / num_threads);
		unsigned long long prev = rr - from * from;
		unsigned long long pixels = ceil(sqrtl(prev));

		for (unsigned long long i = from; i < to - 1; ++i) {
			prev -= (i << 1) + 1;
			pixels += ceil(sqrtl(prev));
		}
		partial_sum[tid] = (pixels + k - ((from + to - 1) * (to - from) >> 1)) % k;
	}

	unsigned long long answer = 0;
	for (int i = 0; i < num_threads; i++)
		answer += partial_sum[i];
	printf("%llu\n", (4 * (2 * answer + k - l)) % k);
	return 0;
}
