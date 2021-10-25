// Author: justin0u0<mail@justin0u0.com>

#include <cstdio>
#include <boost/sort/spreadsort/float_sort.hpp>
#include <mpi.h>

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

int main(int argc, char** argv) {
	int n = atoi(argv[1]);

	MPI_Init(&argc, &argv);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	const int EXPECTED_SEND_TIMES = 8;
	// if too small, no limit
	const int LIMIT = n / size / EXPECTED_SEND_TIMES > 5000000 ? n / size / EXPECTED_SEND_TIMES + 10 : 999999999;

	MPI_File in_file, out_file;
	MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &in_file);

	int m = n / size + (rank < n % size);
	int offset = n / size * rank + min(n % size, rank);
	int lm = m + (rank == n % size); // the left node's size
	int rm = m - (rank + 1 == n % size); // the right node's size
	// printf("rank[%d]: lm: %d, rm: %d\n", rank, lm, rm);

	float* arr = new float[m];
	float* rarr = new float[lm];
	float* tarr = new float[m];
	MPI_File_read_at(in_file, sizeof(float) * offset, arr, m, MPI_FLOAT, MPI_STATUS_IGNORE);
	MPI_File_close(&in_file);
	boost::sort::spreadsort::float_sort(arr, arr + m);

	bool is_left = rank & 1;
	int turn = size + 1;
	bool local_done;
	while(turn--) {
		if (is_left && rank != size - 1 && m > 0 && rm > 0) {
			int to_send = m;  // total to send
			int to_recv = rm; // total to recv
			int sc = 1, rc = 1; // sc: send count, rc: recv count
			float *arr_ptr = arr + m;
			float *rarr_ptr = rarr;

			MPI_Sendrecv(arr_ptr - sc, sc, MPI_FLOAT, rank + 1, 0, rarr_ptr, rc, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			arr_ptr -= sc;
			to_send -= sc;
			rarr_ptr += rc;
			to_recv -= rc;

			local_done = (arr[m - 1] <= rarr[0]);

			if (!local_done) {
				MPI_Request send_r, recv_r;
				int l = 0, r = 0, i = 0;

				while (to_send > 0 || to_recv > 0) {
					// printf("rank[%d]: start from left once: %d, %d\n", rank, to_send, to_recv);
					// Asynchronously send, receive values for next round.
					if (to_send > 0) {
						sc = min(to_send, LIMIT);
						// printf("rank[%d]: send from left: %d, %d\n", rank, (arr_ptr - sc) - arr, sc); fflush(stdin);
						MPI_Isend(arr_ptr - sc, sc, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &send_r);
					}

					if (to_recv > 0) {
						rc = min(to_recv, LIMIT);
						// printf("rank[%d]: receive from left: %d, %d\n", rank, rarr_ptr - rarr, rc); fflush(stdin);
						MPI_Irecv(rarr_ptr, rc, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &recv_r);
					}

					// Process the current round.
					int r_end = rarr_ptr - rarr;
					while (i != m) {
						if (arr[l] < rarr[r]) {
							tarr[i++] = arr[l++];
							if (l == m) break;
						} else {
							tarr[i++] = rarr[r++];
							if (r == r_end) break;
						}
					}

					// Wait until next round data done.
					if (to_send > 0) {
						MPI_Wait(&send_r, MPI_STATUS_IGNORE);
						arr_ptr -= sc;
						to_send -= sc;
					}
					if (to_recv > 0) {
						MPI_Wait(&recv_r, MPI_STATUS_IGNORE);
						rarr_ptr += rc;
						to_recv -= rc;
					}
					// printf("rank[%d]: done from left once: %d, %d\n", rank, to_send, to_recv);

					if (i == m) to_recv = 0;
					MPI_Sendrecv(&to_recv, 1, MPI_INT, rank + 1, 0, &to_send, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}

				// Process the last round.
				int r_end = rarr_ptr - rarr;
				while (i != m) {
					if (arr[l] < rarr[r]) {
						tarr[i++] = arr[l++];
						if (l == m) break;
					} else {
						tarr[i++] = rarr[r++];
						if (r == r_end) break;
					}
				}
				if (r == r_end)
					while (i != m)
						tarr[i++] = arr[l++];
				if (l == m)
					while (i != m)
						tarr[i++] = rarr[r++];
				std::swap(tarr, arr);
			}
		}

		if (!is_left && rank != 0 && m > 0 && lm > 0) {
			int to_send = m;  // total to send
			int to_recv = lm; // total to recv
			int sc = 1, rc = 1; // sc: send count, rc: recv count
			float *arr_ptr = arr;
			float *rarr_ptr = rarr + lm;

			MPI_Sendrecv(arr_ptr, sc, MPI_FLOAT, rank - 1, 0, rarr_ptr - rc, rc, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			arr_ptr += sc;
			to_send -= sc;
			rarr_ptr -= rc;
			to_recv -= rc;

			local_done = (rarr[lm - 1] <= arr[0]);

			if (!local_done) {
				MPI_Request send_r, recv_r;
				int l = m - 1, r = lm - 1, i = m - 1;

				while (to_send > 0 || to_recv > 0) {
					// printf("rank[%d]: start from right once: %d, %d\n", rank, to_send, to_recv);

					// Asynchronously send, receive values for next round.
					if (to_send > 0) {
						sc = min(to_send, LIMIT);
						// printf("rank[%d]: send from right: %d, %d\n", rank, arr_ptr - arr, sc); fflush(stdin);
						MPI_Isend(arr_ptr, sc, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, &send_r);
					}

					if (to_recv > 0) {
						rc = min(to_recv, LIMIT);
						// printf("rank[%d]: receive from right: %d, %d\n", rank, (rarr_ptr - rc) - rarr, rc); fflush(stdin);
						MPI_Irecv(rarr_ptr - rc, rc, MPI_FLOAT, rank - 1, 0, MPI_COMM_WORLD, &recv_r);
					}

					// Process the current round.
					int r_end = rarr_ptr - rarr - 1;
					while (i != -1) {
						if (arr[l] > rarr[r]) {
							tarr[i--] = arr[l--];
							if (l == -1) break;
						} else {
							tarr[i--] = rarr[r--];
							if (r == r_end) break;
						}
					}

					// Wait until next round data done.
					if (to_send > 0) {
						MPI_Wait(&send_r, MPI_STATUS_IGNORE);
						arr_ptr += sc;
						to_send -= sc;
					}
					if (to_recv > 0) {
						MPI_Wait(&recv_r, MPI_STATUS_IGNORE);
						rarr_ptr -= rc;
						to_recv -= rc;
					}
					// printf("rank[%d]: done from right once: %d, %d\n", rank, to_send, to_recv);

					if (i == -1) to_recv = 0;
					MPI_Sendrecv(&to_recv, 1, MPI_INT, rank - 1, 0, &to_send, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}

				// Process the last round.
				int r_end = rarr_ptr - rarr - 1;
				while (i != -1) {
					if (arr[l] > rarr[r]) {
						tarr[i--] = arr[l--];
						if (l == -1) break;
					} else {
						tarr[i--] = rarr[r--];
						if (r == r_end) break;
					}
				}
				if (r == r_end)
					while (i != -1)
						tarr[i--] = arr[l--];
				if (l == -1)
					while (i != -1)
						tarr[i--] = rarr[r--];
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
