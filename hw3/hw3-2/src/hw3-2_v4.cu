// Author: justin0u0<mail@justin0u0.com>
//
// Finished basic blocked floyd warshall.
// Optimization:
//   - Calculate 4 position in a single thread to fully utilize shared memory.

#include <stdio.h>
#include <stdlib.h>

const int INF = ((1 << 30) - 1);
const int BLOCK_SIZE = 64;
const int HBS = 32; // define the half block size

void handleInput(const char* inputFile, int& n, int& m, int& origN, int** hostD) {
	FILE* file = fopen(inputFile, "rb");
	fread(&n, sizeof(int), 1, file);
	fread(&m, sizeof(int), 1, file);

	origN = n;
	if (n % BLOCK_SIZE != 0) {
		n = n + (BLOCK_SIZE - n % BLOCK_SIZE);
	}

	*hostD = (int*)malloc(n * n * sizeof(int));

	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			(*hostD)[i * n + j] = (i == j) ? 0 : INF;
		}
	}

	int edge[3];
	for (int i = 0; i < m; ++i) {
		fread(edge, sizeof(int), 3, file);
		(*hostD)[edge[0] * n + edge[1]] = edge[2];
	}
	fclose(file);
}

void handleOutput(const char* outputFile, const int n, const int origN, int* hostD) {
	FILE* file = fopen(outputFile, "w");
	for (int i = 0; i < origN; ++i) {
		fwrite(hostD + i * n, sizeof(int), origN, file);
	}
	// fwrite(hostD, sizeof(int), n * n, file);
	fclose(file);
}

__global__ void blockedFloydWarshallPhase1(int n, int blockId, int* d, size_t pitch) {
	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	// load the block into shared memory
	int i = y + blockId * BLOCK_SIZE;
	int j = x + blockId * BLOCK_SIZE;

	__shared__ int cacheD[BLOCK_SIZE][BLOCK_SIZE];
	cacheD[y][x] = d[i * pitch + j];
	cacheD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];
	__syncthreads();

	// compute phase 1 - dependent phase
	#pragma unroll 32
	for (int k = 0; k < BLOCK_SIZE; ++k) {
		// cacheD[y][x] = min(cacheD[y][x], cacheD[y][k] + cacheD[k][x]);
		cacheD[y][x] = (cacheD[y][x] < cacheD[y][k] + cacheD[k][x]) ? cacheD[y][x] : cacheD[y][k] + cacheD[k][x];
		cacheD[y + HBS][x] = (cacheD[y + HBS][x] < cacheD[y + HBS][k] + cacheD[k][x]) ? cacheD[y + HBS][x] : cacheD[y + HBS][k] + cacheD[k][x];
		cacheD[y][x + HBS] = (cacheD[y][x + HBS] < cacheD[y][k] + cacheD[k][x + HBS]) ? cacheD[y][x + HBS] : cacheD[y][k] + cacheD[k][x + HBS];
		cacheD[y + HBS][x + HBS] = (cacheD[y + HBS][x + HBS] < cacheD[y + HBS][k] + cacheD[k][x + HBS]) ? cacheD[y + HBS][x + HBS] : cacheD[y + HBS][k] + cacheD[k][x + HBS];

		__syncthreads();
	}

	// load shared memory back to the global memory
	d[i * pitch + j] = cacheD[y][x];
	d[(i + HBS) * pitch + j] = cacheD[y + HBS][x];
	d[i * pitch + (j + HBS)] = cacheD[y][x + HBS];
	d[(i + HBS) * pitch + (j + HBS)] = cacheD[y + HBS][x + HBS];
}

__global__ void blockedFloydWarshallPhase2(int n, int blockId, int* d, size_t pitch) {
	// skipping the base block (from phase 1)
	if (blockIdx.x == blockId) return;

	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	// load the base block into shared memory
	int i = y + blockId * BLOCK_SIZE;
	int j = x + blockId * BLOCK_SIZE;

	__shared__ int cacheBaseD[BLOCK_SIZE][BLOCK_SIZE];
	cacheBaseD[y][x] = d[i * pitch + j];
	cacheBaseD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheBaseD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheBaseD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];

	// load the block into shared memory
	if (blockIdx.y == 0) {
		j = x + blockIdx.x * BLOCK_SIZE;
	} else {
		i = y + blockIdx.x * BLOCK_SIZE;
	}

	__shared__ int cacheD[BLOCK_SIZE][BLOCK_SIZE];
	cacheD[y][x] = d[i * pitch + j];
	cacheD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];
	__syncthreads();

	// compute phase 2 - partial dependent phase
	if (blockIdx.y == 0) {
		#pragma unroll 32
		for (int k = 0; k < BLOCK_SIZE; ++k) {
			// cacheD[y][x] = min(cacheD[y][x], cacheBaseD[y][k] + cacheD[k][x]);
			cacheD[y][x] = (cacheD[y][x] < cacheBaseD[y][k] + cacheD[k][x]) ? cacheD[y][x] : cacheBaseD[y][k] + cacheD[k][x];
			cacheD[y + HBS][x] = (cacheD[y + HBS][x] < cacheBaseD[y + HBS][k] + cacheD[k][x]) ? cacheD[y + HBS][x] : cacheBaseD[y + HBS][k] + cacheD[k][x];
			cacheD[y][x + HBS] = (cacheD[y][x + HBS] < cacheBaseD[y][k] + cacheD[k][x + HBS]) ? cacheD[y][x + HBS] : cacheBaseD[y][k] + cacheD[k][x + HBS];
			cacheD[y + HBS][x + HBS] = (cacheD[y + HBS][x + HBS] < cacheBaseD[y + HBS][k] + cacheD[k][x + HBS]) ? cacheD[y + HBS][x + HBS] : cacheBaseD[y + HBS][k] + cacheD[k][x + HBS];
		}
	} else {
		#pragma unroll 32
		for (int k = 0; k < BLOCK_SIZE; ++k) {
			// cacheD[y][x] = min(cacheD[y][x], cacheD[y][k] + cacheBaseD[k][x]);
			cacheD[y][x] = (cacheD[y][x] < cacheD[y][k] + cacheBaseD[k][x]) ? cacheD[y][x] : cacheD[y][k] + cacheBaseD[k][x];
			cacheD[y + HBS][x] = (cacheD[y + HBS][x] < cacheD[y + HBS][k] + cacheBaseD[k][x]) ? cacheD[y + HBS][x] : cacheD[y + HBS][k] + cacheBaseD[k][x];
			cacheD[y][x + HBS] = (cacheD[y][x + HBS] < cacheD[y][k] + cacheBaseD[k][x + HBS]) ? cacheD[y][x + HBS] : cacheD[y][k] + cacheBaseD[k][x + HBS];
			cacheD[y + HBS][x + HBS] = (cacheD[y + HBS][x + HBS] < cacheD[y + HBS][k] + cacheBaseD[k][x + HBS]) ? cacheD[y + HBS][x + HBS] : cacheD[y + HBS][k] + cacheBaseD[k][x + HBS];
		}
	}

	// load shared memory back to the global memory
	d[i * pitch + j] = cacheD[y][x];
	d[(i + HBS) * pitch + j] = cacheD[y + HBS][x];
	d[i * pitch + (j + HBS)] = cacheD[y][x + HBS];
	d[(i + HBS) * pitch + (j + HBS)] = cacheD[y + HBS][x + HBS];
}

__global__ void blockedFloydWarshallPhase3(int n, int blockId, int* d, size_t pitch) {
	// skipping the base blocks (from phase 1, 2)
	if (blockIdx.x == blockId || blockIdx.y == blockId) return;

	// x: [0, BLOCK_SIZE), y: [0, BLOCK_SIZE)
	int x = threadIdx.x;
	int y = threadIdx.y;

	int i; // y + blockIdx.y * BLOCK_SIZE;
	int j; // x + blockIdx.x * BLOCK_SIZE;

	// load the base column block (same row) into shared memory
	__shared__ int cacheBaseColD[BLOCK_SIZE][BLOCK_SIZE];
	i = y + blockIdx.y * BLOCK_SIZE;
	j = x + blockId * BLOCK_SIZE;
	cacheBaseColD[y][x] = d[i * pitch + j];
	cacheBaseColD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheBaseColD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheBaseColD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];

	// load the base row block (same column) into shared memory
	__shared__ int cacheBaseRowD[BLOCK_SIZE][BLOCK_SIZE];
	i = y + blockId * BLOCK_SIZE;
	j = x + blockIdx.x * BLOCK_SIZE;
	cacheBaseRowD[y][x] = d[i * pitch + j];
	cacheBaseRowD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheBaseRowD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheBaseRowD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];
	__syncthreads();

	// compute phase 3 - independence phase
	__shared__ int cacheD[BLOCK_SIZE][BLOCK_SIZE];
	i = y + blockIdx.y * BLOCK_SIZE;
	j = x + blockIdx.x * BLOCK_SIZE;
	cacheD[y][x] = d[i * pitch + j];
	cacheD[y + HBS][x] = d[(i + HBS) * pitch + j];
	cacheD[y][x + HBS] = d[i * pitch + (j + HBS)];
	cacheD[y + HBS][x + HBS] = d[(i + HBS) * pitch + (j + HBS)];

	#pragma unroll 32
	for (int k = 0; k < BLOCK_SIZE; ++k) {
		// cacheD[y][x] = min(cacheD[y][x], cacheBaseColD[y][k] + cacheBaseRowD[k][x]);
		cacheD[y][x] = (cacheD[y][x] < cacheBaseColD[y][k] + cacheBaseRowD[k][x]) ? cacheD[y][x] : cacheBaseColD[y][k] + cacheBaseRowD[k][x];
		cacheD[y + HBS][x] = (cacheD[y + HBS][x] < cacheBaseColD[y + HBS][k] + cacheBaseRowD[k][x]) ? cacheD[y + HBS][x] : cacheBaseColD[y + HBS][k] + cacheBaseRowD[k][x];
		cacheD[y][x + HBS] = (cacheD[y][x + HBS] < cacheBaseColD[y][k] + cacheBaseRowD[k][x + HBS]) ? cacheD[y][x + HBS] : cacheBaseColD[y][k] + cacheBaseRowD[k][x + HBS];
		cacheD[y + HBS][x + HBS] = (cacheD[y + HBS][x + HBS] < cacheBaseColD[y + HBS][k] + cacheBaseRowD[k][x + HBS]) ? cacheD[y + HBS][x + HBS] : cacheBaseColD[y + HBS][k] + cacheBaseRowD[k][x + HBS];
	}

	// load new distance back to the global memory
	d[i * pitch + j] = cacheD[y][x];
	d[(i + HBS) * pitch + j] = cacheD[y + HBS][x];
	d[i * pitch + (j + HBS)] = cacheD[y][x + HBS];
	d[(i + HBS) * pitch + (j + HBS)] = cacheD[y + HBS][x + HBS];
}

int main(int argc, char** argv) {
	int n, m, origN;

	int* hostD;
	handleInput(argv[1], n, m, origN, &hostD);

	// pinned the host memory to accerlate cudaMemcpy
	cudaHostRegister(hostD, n * n * sizeof(int), cudaHostAllocDefault);

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

	/* blocked floyd warshall */

	// number of blocks is numberOfBlocks * numberOfBlocks
	// int numberOfBlocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int numberOfBlocks = n / BLOCK_SIZE;

	dim3 gridPhase1(1, 1);
	dim3 gridPhase2(numberOfBlocks, 2); // the 2 represents the row & the column respectively
	dim3 gridPhase3(numberOfBlocks, numberOfBlocks);
	dim3 threadsPerBlock(32, 32);

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

	handleOutput(argv[2], n, origN, hostD);
	delete[] hostD;

	return 0;
}
