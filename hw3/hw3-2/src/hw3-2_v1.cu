// Author: justin0u0<mail@justin0u0.com>
//
// Finished basic blocked floyd warshall algorithm with no optimization

#include <stdio.h>
#include <stdlib.h>

const int INF = ((1 << 30) - 1);
const int BLOCK_SIZE = 32;

void handleInput(const char* inputFile, int& n, int& m, int** hostD) {
	FILE* file = fopen(inputFile, "rb");
	fread(&n, sizeof(int), 1, file);
	fread(&m, sizeof(int), 1, file);

	*hostD = (int*)malloc(n * n * sizeof(int));

	int** d = (int**)malloc(n * sizeof(int*));
	for (int i = 0; i < n; ++i) {
		d[i] = (*hostD) + i * n;
		for (int j = 0; j < n; ++j) {
			if (i == j) {
				d[i][j] = 0;
			} else {
				d[i][j] = INF;
			}
		}
	}

	int edge[3];
	for (int i = 0; i < m; ++i) {
		fread(edge, sizeof(int), 3, file);
		d[edge[0]][edge[1]] = edge[2];
	}
	fclose(file);
}

void handleOutput(const char* outputFile, const int n, int* hostD) {
	FILE* file = fopen(outputFile, "w");
	fwrite(hostD, sizeof(int), n * n, file);
	fclose(file);
}

__global__ void naiveFloydWarshall(const int n, const int k, int* d) {
	int x = threadIdx.x + blockIdx.x * blockDim.x;
	int y = threadIdx.y + blockIdx.y * blockDim.y;

	if (x >= n || y >= n) return;

	int idxIJ = x * n + y;
	int idxIK = x * n + k;
	int idxKJ = k * n + y;

	/* no cache, 6 global memory access */

	/*
		if (d[idxIJ] > d[idxIK] + d[idxKJ]) {
			d[idxIJ] = d[idxIK] + d[idxKJ];
		}
	*/

	/* shared memory cached, 2 global memory access */

	__shared__ int cacheIJ[BLOCK_SIZE][BLOCK_SIZE];
	__shared__ int cacheIK[BLOCK_SIZE][BLOCK_SIZE];
	__shared__ int cacheKJ[BLOCK_SIZE][BLOCK_SIZE];
	cacheIJ[threadIdx.x][threadIdx.y] = d[idxIJ];
	cacheIK[threadIdx.x][threadIdx.y] = d[idxIK];
	cacheKJ[threadIdx.x][threadIdx.y] = d[idxKJ];
	__syncthreads();

	if (cacheIJ[threadIdx.x][threadIdx.y] > cacheIK[threadIdx.x][threadIdx.y] + cacheKJ[threadIdx.x][threadIdx.y]) {
		d[idxIJ] = cacheIK[threadIdx.x][threadIdx.y] + cacheKJ[threadIdx.x][threadIdx.y];
	}
}

__global__ void blockedFloydWarshallPhase1(int n, int blockId, int* d, size_t pitch) {
	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	// load the block into shared memory
	int i = y + blockId * BLOCK_SIZE;
	int j = x + blockId * BLOCK_SIZE;
	int idxIJ = i * pitch + j;

	__shared__ int cacheD[BLOCK_SIZE][BLOCK_SIZE];
	cacheD[y][x] = (i < n && j < n) ? d[idxIJ] : INF;
	__syncthreads();

	// compute phase 1 - dependent phase
	#pragma unroll
	for (int k = 0; k < BLOCK_SIZE; ++k) {
		cacheD[y][x] = min(cacheD[y][x], cacheD[y][k] + cacheD[k][x]);

		__syncthreads();
	}

	// load shared memory back to the global memory
	if (i < n && j < n) {
		d[idxIJ] = cacheD[y][x];
	}
}

__global__ void blockedFloydWarshallPhase2(int n, int blockId, int* d, size_t pitch) {
	// skipping the base block (from phase 1)
	if (blockIdx.x == blockId) return;

	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	// blockIdx.y: [0, 1]
	// isRow is true if the block has same index i with the base block
	bool isRow = (blockIdx.y == 0);

	// load the base block into shared memory
	int i = y + blockId * BLOCK_SIZE;
	int j = x + blockId * BLOCK_SIZE;
	int idxIJ = i * pitch + j;

	__shared__ int cacheBaseD[BLOCK_SIZE][BLOCK_SIZE];
	cacheBaseD[y][x] = (i < n && j < n) ? d[idxIJ] : INF;

	// load the block into shared memory
	if (isRow) {
		j = x + blockIdx.x * BLOCK_SIZE;
	} else {
		i = y + blockIdx.x * BLOCK_SIZE;
	}
	idxIJ = i * pitch + j;

	__shared__ int cacheD[BLOCK_SIZE][BLOCK_SIZE];
	cacheD[y][x] = (i < n && j < n) ? d[idxIJ] : INF;
	__syncthreads();

	// compute phase 2 - partial dependent phase
	if (isRow) {
		#pragma unroll
		for (int k = 0; k < BLOCK_SIZE; ++k) {
			cacheD[y][x] = min(cacheD[y][x], cacheBaseD[y][k] + cacheD[k][x]);

			__syncthreads();
		}
	} else {
		#pragma unroll
		for (int k = 0; k < BLOCK_SIZE; ++k) {
			cacheD[y][x] = min(cacheD[y][x], cacheD[y][k] + cacheBaseD[k][x]);

			__syncthreads();
		}
	}

	// load shared memory back to the global memory
	if (i < n && j < n) {
		d[idxIJ] = cacheD[y][x];
	}
}

__global__ void blockedFloydWarshallPhase3(int n, int blockId, int* d, size_t pitch) {
	// skipping the base blocks (from phase 1, 2)
	if (blockIdx.x == blockId || blockIdx.y == blockId) return;

	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	int i = y + blockIdx.y * BLOCK_SIZE;
	int j = x + blockIdx.x * BLOCK_SIZE;
	int idxIJ = i * pitch + j;

	// load the base column block (same row) into shared memory
	__shared__ int cacheBaseColD[BLOCK_SIZE][BLOCK_SIZE];
	int baseJ = x + blockId * BLOCK_SIZE;
	cacheBaseColD[y][x] = (i < n && baseJ < n) ? d[i * pitch + baseJ] : INF;

	// load the base row block (same column) into shared memory
	__shared__ int cacheBaseRowD[BLOCK_SIZE][BLOCK_SIZE];
	int baseI = y + blockId * BLOCK_SIZE;
	cacheBaseRowD[y][x] = (baseI < n && j < n) ? d[baseI * pitch + j] : INF;
	__syncthreads();

	// compute phase 3 - independence phase

	if (i < n && j < n) {
		int curDist = d[idxIJ];

		#pragma unroll
		for (int k = 0; k < BLOCK_SIZE; ++k) {
			curDist = min(curDist, cacheBaseColD[y][k] + cacheBaseRowD[k][x]);
		}

		// load new distance back to the global memory
		d[idxIJ] = curDist;
	}
}

int main(int argc, char** argv) {
	int n, m;

	int* hostD;
	handleInput(argv[1], n, m, &hostD);

	// pinned the host memory to accerlate cudaMemcpy
	cudaHostRegister(hostD, n * n * sizeof(int), cudaHostAllocMapped);

	int* deviceD;
	/* zero copy */
	// cudaHostGetDevicePointer((void**)&deviceD, (void*)hostD, cudaHostRegisterDefault);

	/* normal cuda malloc + memcpy */
	// cudaMalloc((void**)&deviceD, n * n * sizeof(int));
	// cudaMemcpy(deviceD, hostD, n * n * sizeof(int), cudaMemcpyHostToDevice);

	/* cudaMallocPitch + cudaMemcpy2D */
	size_t pitch;
	cudaMallocPitch((void**)&deviceD, &pitch, n * sizeof(int), n);
	cudaMemcpy2D(deviceD, pitch, hostD, n * sizeof(int), n * sizeof(int), n, cudaMemcpyHostToDevice);

	/* naive floyd warshall */

	/*
	dim3 threadsPerBlock(BLOCK_SIZE, BLOCK_SIZE);
	dim3 numberOfBlocks((n + BLOCK_SIZE - 1) / BLOCK_SIZE, (n + BLOCK_SIZE - 1) / BLOCK_SIZE);
	printf("(%d, %d) (%d, %d)\n",
		threadsPerBlock.x, threadsPerBlock.y,
		numberOfBlocks.x, numberOfBlocks.y);
	for (int k = 0; k < n; ++k) {
		naiveFloydWarshall<<<numberOfBlocks, threadsPerBlock>>>(n, k, deviceD);
	}
	*/

	/* blocked floyd warshall */

	// number of blocks is numberOfBlocks * numberOfBlocks
	int numberOfBlocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;

	dim3 gridPhase1(1, 1);
	dim3 gridPhase2(numberOfBlocks, 2); // the 2 represents the row & the column respectively
	dim3 gridPhase3(numberOfBlocks, numberOfBlocks);
	dim3 threadsPerBlock(BLOCK_SIZE, BLOCK_SIZE);

	for (int blockId = 0; blockId < numberOfBlocks; ++blockId) {
		blockedFloydWarshallPhase1<<<gridPhase1, threadsPerBlock>>>(n, blockId, deviceD, pitch / sizeof(int));
		blockedFloydWarshallPhase2<<<gridPhase2, threadsPerBlock>>>(n, blockId, deviceD, pitch / sizeof(int));
		blockedFloydWarshallPhase3<<<gridPhase3, threadsPerBlock>>>(n, blockId, deviceD, pitch / sizeof(int));
	}

	/* zero copy */

	/* normal cuda memcpy */
	// cudaMemcpy(hostD, deviceD, n * n * sizeof(int), cudaMemcpyDeviceToHost);

	/* cudaMemcpy2D */
	cudaMemcpy2D(hostD, n * sizeof(int), deviceD, pitch, n * sizeof(int), n, cudaMemcpyDeviceToHost);

	cudaFree(deviceD);

	handleOutput(argv[2], n, hostD);

	return 0;
}
