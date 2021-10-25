// Author: justin0u0<mail@justin0u0.com>

#include <cstdio>
#include <boost/sort/spreadsort/float_sort.hpp>
#include <mpi.h>

#define min(a, b) (a < b ? a : b)

void handle_receive(float *left_arr, float *right_arr, float *temp_arr, int left_m, int right_m) {
	int l = 0, r = 0;
	for (int i = 0; i < left_m + right_m; ++i) {
		if (r == right_m || (l < left_m && left_arr[l] < right_arr[r]))
			temp_arr[i] = left_arr[l++];
		else
			temp_arr[i] = right_arr[r++];
	}
}

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
	int rm = m + (rank == n % size); // the receiver node's m value

	float* arr = new float[m];
	float* rarr = new float[rm]; // the received array
	float* tarr = new float[m + rm];
	MPI_File_read_at(in_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	boost::sort::spreadsort::float_sort(arr, arr + m);

	bool is_sender = rank & 1;
	int turn = size + 1;
	bool local_done, done;
	while(turn--) {
		if (is_sender && rank != size - 1 && m > 0) {
			MPI_Sendrecv(&arr[m - 1], 1, MPI_FLOAT, rank + 1, 0, rarr, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (arr[m - 1] < rarr[0]);

			if (!local_done) {
				MPI_Send(arr, m, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD);
				MPI_Recv(arr, m, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
		}

		if (!is_sender && rank != 0 && rm > 0) {
			MPI_Sendrecv(arr, 1, MPI_FLOAT, rank - 1, 0, rarr, 1, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			local_done = (rarr[0] < arr[0]);

			if (!local_done) {
				MPI_Recv(rarr, rm, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				handle_receive(rarr, arr, tarr, rm, m);
				MPI_Send(tarr, rm, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD);
				std::copy(&tarr[rm], &tarr[rm + m], arr);
			}
		}

		MPI_Allreduce(&local_done, &done, 1, MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD);
		if (done) break;

		is_sender ^= 1;
	}

	MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &out_file);
	MPI_File_write_at(out_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	MPI_Finalize();
}
