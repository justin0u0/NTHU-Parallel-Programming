#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <assert.h>
#include <omp.h>

int main(int argc, char** argv) {
	assert(argc == 3);

	unsigned long long radius = atoll(argv[1]);
	unsigned long long k = atoll(argv[2]);
	unsigned long long rr = radius * radius;
	unsigned long long l = ceil(radius / sqrt(2));

	int rank, size;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	unsigned long long from = rank * (l / size);
	unsigned long long to = (rank == size - 1) ? l : (rank + 1) * (l / size);
	unsigned long long local_sum = 0;

	#pragma omp parallel
	{
		unsigned long long i;
		unsigned long long pixels = 0;

		#pragma omp for
			for (i = from; i < to; ++i) {
				pixels += ceil(sqrtl(rr - i * i)) - i;
			}
		
		pixels %= k;
		
		#pragma omp critical
			local_sum += pixels;
		
		/*
		unsigned long long prev = rr - from * from;
		unsigned long long pixels = ceil(sqrtl(prev));
		for (unsigned long long i = from; i < to - 1; ++i) {
			prev -= (i << 1) + 1;
			pixels += ceil(sqrtl(prev));
		}
		pixels = (pixels + k - ((from + to - 1) * (to - from) >> 1)) % k;
		*/
	}

	unsigned long long sum = 0;
	MPI_Reduce(&local_sum, &sum, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Finalize();

	if (rank == 0) {
		printf("%llu\n", (4 * (2 * sum + k - l)) % k);
	}
	return 0;
}
