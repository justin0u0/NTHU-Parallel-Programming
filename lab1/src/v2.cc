// Author: justin0u0<mail@justin0u0.com>

#include <stdio.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char** argv) {
	if (argc != 3) {
		fprintf(stderr, "must provide exactly 2 arguments!\n");
		return 1;
	}

	unsigned long long radius = atoll(argv[1]);
	unsigned long long k = atoll(argv[2]);
	unsigned long long pixels = 0;

	unsigned long long l = 0, r = radius + 1, x;
	while (l < r) {
		x = (l + r) >> 1;
		// y = ceil(sqrt(radius * radius - x * x));
		if (ceil(sqrtl(radius * radius - x * x)) >= x + 1)
			l = x + 1;
		else
			r = x;
	}

	int rank, size;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	for (unsigned long long i = rank; i < l; i += size) {
		pixels += ceil(sqrtl(radius * radius - i * i)) - i;
	}
	pixels %= k;

	unsigned long long sum = 0;
	MPI_Reduce(&pixels, &sum, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Finalize();

	if (rank == 0) {
		unsigned long long dupl = ceil(radius / sqrtl(2.0));
		printf("%llu\n", (4 * (2 * sum + k - dupl)) % k);
	}
	return 0;
}
