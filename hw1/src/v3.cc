// Author: justin0u0<mail@justin0u0.com>

#include <cstdio>
#include <boost/sort/spreadsort/float_sort.hpp>
#include <mpi.h>

#define min(a, b) (a < b ? a : b)

int main(int argc, char** argv) {
	int n = atoi(argv[1]);

	MPI_Init(&argc, &argv);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	MPI_File in_file, out_file;
	MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &in_file);

	int m = n / size + (rank < n % size);
	int offset = n / size * rank + min(n % size, rank);
	int lm = m + (rank == n % size); // the left node's size
	int rm = m - (rank + 1 == n % size); // the right node's size

	float* arr = new float[m];
	float* rarr = new float[lm];
	float* tarr = new float[m];
	MPI_File_read_at(in_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	MPI_File_close(&in_file);
	boost::sort::spreadsort::float_sort(arr, arr + m);

	bool is_left = rank & 1;
	int turn = size + 1;
	bool local_done, done;
	while(turn--) {
		if (is_left && rank != size - 1 && m > 0 && rm > 0) {
			MPI_Sendrecv(&arr[m - 1], 1, MPI_FLOAT, rank + 1, 0, rarr, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (arr[m - 1] < rarr[0]);

			if (!local_done) {
				MPI_Sendrecv(arr, m, MPI_FLOAT, rank + 1, 0, rarr, rm, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				int l = 0, r = 0;
				for (int i = 0; i < m; ++i) {
					if (r == rm || (l < m && arr[l] < rarr[r])) tarr[i] = arr[l++];
					else tarr[i] = rarr[r++];
				}
				std::copy(tarr, tarr + m, arr);
			}
		}

		if (!is_left && rank != 0 && m > 0 && lm > 0) {
			MPI_Sendrecv(arr, 1, MPI_FLOAT, rank - 1, 0, rarr, 1, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (rarr[0] < arr[0]);

			if (!local_done) {
				MPI_Sendrecv(arr, m, MPI_FLOAT, rank - 1, 0, rarr, lm, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				int l = m - 1, r = lm - 1;
				for (int i = m - 1; i >= 0; --i) {
					if (r == -1 || (l > -1 && arr[l] > rarr[r])) tarr[i] = arr[l--];
					else tarr[i] = rarr[r--];
				}
				std::copy(tarr, tarr + m, arr);
			}
		}

		MPI_Allreduce(&local_done, &done, 1, MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD);
		if (done) break;

		is_left ^= 1;
	}

	MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &out_file);
	MPI_File_write_at(out_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	MPI_File_close(&out_file);
	MPI_Finalize();
}
