#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
	assert(argc == 3);

	unsigned long long r = atoll(argv[1]);
	unsigned long long k = atoll(argv[2]);
	unsigned long long rr = r * r;

	int num_threads;
	unsigned long long* partial_sum;
	unsigned long long chuck;

	#pragma omp parallel
	{
		#pragma single
		{
			num_threads = omp_get_num_threads();
			partial_sum = new unsigned long long[num_threads];
			chuck = r / num_threads + 1;
		}

		int tid = omp_get_thread_num();

		unsigned long long x;
		unsigned long long pixels = 0;
		unsigned long long cur = rr - (tid * chuck) * (tid * chuck);

		#pragma omp for schedule(static, chuck)
			for (x = 0; x < r; ++x) {
				pixels += ceil(sqrtl(cur));
				cur -= (x << 1) + 1;
			}

		pixels %= k;
		partial_sum[tid] = pixels;
	}

	unsigned long long answer = 0;
	for (int i = 0; i < num_threads; i++)
		answer += partial_sum[i];
	printf("%llu\n", (4 * answer) % k);
	return 0;
}
