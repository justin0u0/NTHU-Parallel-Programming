#include <iostream>
#include <cstdlib>
#include <cassert>
#include <zlib.h>
#include <png.h>

#define MASK_N 2
#define MASK_X 5
#define MASK_Y 5
#define SCALE 8
#define X_BOUND 2
#define Y_BOUND 2

__constant__ char mask[MASK_N][MASK_X][MASK_Y] = { 
	{{ -1, -4, -6, -4, -1},
	{ -2, -8,-12, -8, -2},
	{  0,  0,  0,  0,  0}, 
	{  2,  8, 12,  8,  2}, 
	{  1,  4,  6,  4,  1}},
	{{ -1, -2,  0,  2,  1}, 
	{ -4, -8,  0,  8,  4}, 
	{ -6,-12,  0, 12,  6}, 
	{ -4, -8,  0,  8,  4}, 
	{ -1, -2,  0,  2,  1}} 
};

int read_png(const char* filename, unsigned char** image, unsigned* height, unsigned* width, unsigned* channels) {
	unsigned char sig[8];
	FILE* infile;
	infile = fopen(filename, "rb");

	fread(sig, 1, 8, infile);
	if (!png_check_sig(sig, 8))
			return 1;   /* bad signature */

	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return 4;   /* out of memory */

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return 4;   /* out of memory */
	}

	png_init_io(png_ptr, infile);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type, NULL, NULL, NULL);

	png_uint_32  i, rowbytes;
	png_bytep  row_pointers[*height];
	png_read_update_info(png_ptr, info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	*channels = (int)png_get_channels(png_ptr, info_ptr);

	if ((*image = (unsigned char *) malloc(rowbytes * *height)) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 3;
	}

	for (i = 0; i < *height; ++i)
		row_pointers[i] = *image + i * rowbytes;
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);
	return 0;
}

void write_png(const char* filename, png_bytep image, const unsigned height, const unsigned width, const unsigned channels) {
	FILE* fp = fopen(filename, "wb");
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, fp);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
							PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
							PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_filter(png_ptr, 0, PNG_NO_FILTERS);
	png_write_info(png_ptr, info_ptr);
	png_set_compression_level(png_ptr, 1);

	png_bytep row_ptr[height];
	for (int i = 0; i < height; ++ i) {
		row_ptr[i] = image + i * width * channels * sizeof(unsigned char);
	}
	png_write_image(png_ptr, row_ptr);
	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
}

__global__ void sobel(unsigned char* s, unsigned char* t, unsigned height, unsigned width, unsigned channels) {
	int val[MASK_N][3];

	int x = threadIdx.x + blockIdx.x * blockDim.x;
	if (x >= width) return;

	__shared__ unsigned char sR[5][260];
	__shared__ unsigned char sG[5][260];
	__shared__ unsigned char sB[5][260];

	for (int y = 0; y < height; ++y) {
		if (y == 0) {
			for (int v = -Y_BOUND; v <= Y_BOUND; ++v) {
				if (y + v >= 0 && y + v < height) {
					int idx = channels * (width * (y + v) + x);
					sR[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx + 2];
					sG[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx + 1];
					sB[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx];

					if (threadIdx.x == 0) {
						if (x - 2 >= 0) {
							int idx1 = channels * (width * (y + v) + (x - 2));
							sR[v + Y_BOUND][0] = s[idx1 + 2];
							sG[v + Y_BOUND][0] = s[idx1 + 1];
							sB[v + Y_BOUND][0] = s[idx1];
						}
						if (x - 1 >= 0) {
							int idx1 = channels * (width * (y + v) + (x - 1));
							sR[v + Y_BOUND][1] = s[idx1 + 2];
							sG[v + Y_BOUND][1] = s[idx1 + 1];
							sB[v + Y_BOUND][1] = s[idx1];
						}
					}

					if (threadIdx.x == blockDim.x - 1) {
						if (x + 1 < width) {
							int idx1 = channels * (width * (y + v) + (x + 1));
							sR[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1 + 2];
							sG[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1 + 1];
							sB[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1];
						}
						if (x + 2 < width) {
							int idx1 = channels * (width * (y + v) + (x + 2));
							sR[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1 + 2];
							sG[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1 + 1];
							sB[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1];
						}
					}
				}
			}
		} else {
			for (int i = 0; i < 4; i++) {
				sR[i][threadIdx.x + X_BOUND] = sR[i + 1][threadIdx.x + X_BOUND];
				sG[i][threadIdx.x + X_BOUND] = sG[i + 1][threadIdx.x + X_BOUND];
				sB[i][threadIdx.x + X_BOUND] = sB[i + 1][threadIdx.x + X_BOUND];
				if (threadIdx.x == 0) {
					sR[i][0] = sR[i + 1][0];
					sG[i][0] = sG[i + 1][0];
					sB[i][0] = sB[i + 1][0];
					sR[i][1] = sR[i + 1][1];
					sG[i][1] = sG[i + 1][1];
					sB[i][1] = sB[i + 1][1];
				}
				if (threadIdx.x == blockDim.x - 1) {
					sR[i][threadIdx.x + X_BOUND + 1] = sR[i + 1][threadIdx.x + X_BOUND + 1];
					sG[i][threadIdx.x + X_BOUND + 1] = sG[i + 1][threadIdx.x + X_BOUND + 1];
					sB[i][threadIdx.x + X_BOUND + 1] = sB[i + 1][threadIdx.x + X_BOUND + 1];
					sR[i][threadIdx.x + X_BOUND + 2] = sR[i + 1][threadIdx.x + X_BOUND + 2];
					sG[i][threadIdx.x + X_BOUND + 2] = sG[i + 1][threadIdx.x + X_BOUND + 2];
					sB[i][threadIdx.x + X_BOUND + 2] = sB[i + 1][threadIdx.x + X_BOUND + 2];
				}
			}

			int v = Y_BOUND;
			if (y + v >= 0 && y + v < height) {
				int idx = channels * (width * (y + v) + x);
				sR[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx + 2];
				sG[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx + 1];
				sB[v + Y_BOUND][threadIdx.x + X_BOUND] = s[idx];

				if (threadIdx.x == 0) {
					if (x - 2 >= 0) {
						int idx1 = channels * (width * (y + v) + (x - 2));
						sR[v + Y_BOUND][0] = s[idx1 + 2];
						sG[v + Y_BOUND][0] = s[idx1 + 1];
						sB[v + Y_BOUND][0] = s[idx1];
					}
					if (x - 1 >= 0) {
						int idx1 = channels * (width * (y + v) + (x - 1));
						sR[v + Y_BOUND][1] = s[idx1 + 2];
						sG[v + Y_BOUND][1] = s[idx1 + 1];
						sB[v + Y_BOUND][1] = s[idx1];
					}
				}

				if (threadIdx.x == blockDim.x - 1) {
					if (x + 1 < width) {
						int idx1 = channels * (width * (y + v) + (x + 1));
						sR[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1 + 2];
						sG[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1 + 1];
						sB[v + Y_BOUND][threadIdx.x + X_BOUND + 1] = s[idx1];
					}
					if (x + 2 < width) {
						int idx1 = channels * (width * (y + v) + (x + 2));
						sR[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1 + 2];
						sG[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1 + 1];
						sB[v + Y_BOUND][threadIdx.x + X_BOUND + 2] = s[idx1];
					}
				}
			}
		}

		__syncthreads();

		for (int i = 0; i < MASK_N; ++i) {
			val[i][2] = 0;
			val[i][1] = 0;
			val[i][0] = 0;

			for (int v = -Y_BOUND; v <= Y_BOUND; ++v) {
				for (int u = -X_BOUND; u <= X_BOUND; ++u) {
					if ((x + u) >= 0 && (x + u) < width && y + v >= 0 && y + v < height) {
						int idx = threadIdx.x + u + X_BOUND;
						const unsigned char R = sR[v + Y_BOUND][idx];
						const unsigned char G = sG[v + Y_BOUND][idx];
						const unsigned char B = sB[v + Y_BOUND][idx];

						val[i][2] += R * mask[i][u + X_BOUND][v + Y_BOUND];
						val[i][1] += G * mask[i][u + X_BOUND][v + Y_BOUND];
						val[i][0] += B * mask[i][u + X_BOUND][v + Y_BOUND];
					}
				}
			}
		}

		float totalR = 0;
		float totalG = 0;
		float totalB = 0;
		for (int i = 0; i < MASK_N; ++i) {
			totalR += val[i][2] * val[i][2];
			totalG += val[i][1] * val[i][1];
			totalB += val[i][0] * val[i][0];
		}

		totalR = sqrt(totalR) / SCALE;
		totalG = sqrt(totalG) / SCALE;
		totalB = sqrt(totalB) / SCALE;
		const unsigned char cR = (totalR > 255.0) ? 255 : totalR;
		const unsigned char cG = (totalG > 255.0) ? 255 : totalG;
		const unsigned char cB = (totalB > 255.0) ? 255 : totalB;
		t[channels * (width * y + x) + 2] = cR;
		t[channels * (width * y + x) + 1] = cG;
		t[channels * (width * y + x) + 0] = cB;

		__syncthreads();
	}
}

int main(int argc, char** argv) {
	assert(argc == 3);
	unsigned height, width, channels;
	unsigned char* host_s = NULL;
	read_png(argv[1], &host_s, &height, &width, &channels);
	unsigned char* host_t = (unsigned char*) malloc(height * width * channels * sizeof(unsigned char));

	unsigned char* device_s;
	unsigned char* device_t;
	cudaMalloc((void**)&device_s, height * width * channels * sizeof(unsigned char));
	cudaMalloc((void**)&device_t, height * width * channels * sizeof(unsigned char));

	cudaMemcpy(device_s, host_s, height * width * channels * sizeof(unsigned char), cudaMemcpyHostToDevice);

	const int threads_per_block = 256;
	const int number_of_blocks = (width + threads_per_block - 1) / threads_per_block;
	sobel<<<number_of_blocks, threads_per_block>>>(device_s, device_t, height, width, channels);

	cudaMemcpy(host_t, device_t, height * width * channels * sizeof(unsigned char), cudaMemcpyDeviceToHost);

	write_png(argv[2], host_t, height, width, channels);

	return 0;
}
