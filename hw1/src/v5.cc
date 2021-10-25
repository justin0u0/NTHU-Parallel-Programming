// Author: justin0u0<mail@justin0u0.com>
// Description: v4.cc, and change index iteration to pointer iteration.

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
	float* it_arr;
	float* arr_end;
	float* it_rarr;
	float* rarr_end;
	float* it_tarr;
	float* tarr_end;
	bool local_done;
	while(turn--) {
		if (is_left && rank != size - 1 && m > 0 && rm > 0) {
			MPI_Sendrecv(arr + m - 1, 1, MPI_FLOAT, rank + 1, 0, rarr, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (arr[m - 1] <= rarr[0]);

			if (!local_done) {
				MPI_Sendrecv(arr, m - 1, MPI_FLOAT, rank + 1, 0, rarr + 1, rm - 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				it_arr = arr;
				arr_end = arr + m;
				it_rarr = rarr;
				rarr_end = rarr + rm;
				it_tarr = tarr;
				tarr_end = tarr + m;
				while (it_tarr != tarr_end) {
					if (*it_arr < *it_rarr) {
						*it_tarr++ = *it_arr++;
						if (it_arr == arr_end) break;
					} else {
						*it_tarr++ = *it_rarr++;
						if (it_rarr == rarr_end) break;
					}
				}
				if (it_arr == arr_end)
					while (it_tarr != tarr_end)
						*it_tarr++ = *it_rarr++;
				if (it_rarr == rarr_end)
					while (it_tarr != tarr_end)
						*it_tarr++ = *it_arr++;
				std::swap(tarr, arr);
			}
		}

		if (!is_left && rank != 0 && m > 0 && lm > 0) {
			MPI_Sendrecv(arr, 1, MPI_FLOAT, rank - 1, 0, rarr + lm - 1, 1, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (rarr[lm - 1] <= arr[0]);

			if (!local_done) {
				MPI_Sendrecv(arr + 1, m - 1, MPI_FLOAT, rank - 1, 0, rarr, lm - 1, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				it_arr = arr + m - 1;
				arr_end = arr - 1;
				it_rarr = rarr + lm - 1;
				rarr_end = rarr - 1;
				it_tarr = tarr + m - 1;
				tarr_end = tarr - 1;
				while (it_tarr != tarr_end) {
					if (*it_arr > *it_rarr) {
						*it_tarr-- = *it_arr--;
						if (it_arr == arr_end) break;
					} else {
						*it_tarr-- = *it_rarr--;
						if (it_rarr == rarr_end) break;
					}
				} 
				if (it_arr == arr_end)
					while (it_tarr != tarr_end)
						*it_tarr-- = *it_rarr--;
				if (it_rarr == rarr_end)
					while (it_tarr != tarr_end)
						*it_tarr-- = *it_arr--;
				std::swap(tarr, arr);
			}
		}

		is_left ^= 1;
	}

	MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &out_file);
	MPI_File_write_at(out_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	MPI_File_close(&out_file);
	MPI_Finalize();
}
