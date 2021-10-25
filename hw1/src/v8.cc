// Author: justin0u0<mail@justin0u0.com>

#include <cstdio>
#include <boost/sort/spreadsort/float_sort.hpp>
#include <mpi.h>

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

const int LIMIT = 12341234;

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
	// printf("rank[%d]: lm: %d, rm: %d\n", rank, lm, rm);

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
	float* it_tarr;
	float* tarr_end;
	bool local_done, done;
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
				it_arr = arr;
				arr_end = arr + m;
				it_rarr = rarr;
				it_tarr = tarr;
				tarr_end = tarr + m;

				MPI_Request send_r, recv_r;

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
					while (it_tarr != tarr_end) {
						if (*it_arr < *it_rarr) {
							*it_tarr++ = *it_arr++;
							if (it_arr == arr_end) break;
						} else {
							*it_tarr++ = *it_rarr++;
							if (it_rarr == rarr_ptr) break;
						}
					}

					// Wait until next round data done.
					MPI_Wait(&recv_r, MPI_STATUS_IGNORE);
					MPI_Wait(&send_r, MPI_STATUS_IGNORE);
					arr_ptr -= sc;
					to_send -= sc;
					rarr_ptr += rc;
					to_recv -= rc;
					// printf("rank[%d]: done from left once: %d, %d\n", rank, to_send, to_recv);
				}

				// Process the last round.
				while (it_tarr != tarr_end) {
					if (*it_arr < *it_rarr) {
						*it_tarr++ = *it_arr++;
						if (it_arr == arr_end) break;
					} else {
						*it_tarr++ = *it_rarr++;
						if (it_rarr == rarr_ptr) break;
					}
				}
				while (it_tarr != tarr_end) *it_tarr++ = *it_arr++;
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
				it_arr = arr + m - 1;
				arr_end = arr - 1;
				it_rarr = rarr + lm - 1;
				it_tarr = tarr + m - 1;
				tarr_end = tarr - 1;

				MPI_Request send_r, recv_r;

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
					while (it_tarr != tarr_end) {
						if (*it_arr > *it_rarr) {
							*it_tarr-- = *it_arr--;
							if (it_arr == arr_end) break;
						} else {
							*it_tarr-- = *it_rarr--;
							if (it_rarr < rarr_ptr) break;
						}
					}

					// Wait until next round data done.
					MPI_Wait(&recv_r, MPI_STATUS_IGNORE);
					MPI_Wait(&send_r, MPI_STATUS_IGNORE);
					arr_ptr += sc;
					to_send -= sc;
					rarr_ptr -= rc;
					to_recv -= rc;
					// printf("rank[%d]: done from right once: %d, %d\n", rank, to_send, to_recv);
				}

				// Process the last round.
				while (it_tarr != tarr_end) {
					if (*it_arr > *it_rarr) {
						*it_tarr-- = *it_arr--;
						if (it_arr == arr_end) break;
					} else {
						*it_tarr-- = *it_rarr--;
						if (it_rarr < rarr_ptr) break;
					}
				}
				while (it_tarr != tarr_end) *it_tarr-- = *it_arr--;
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
