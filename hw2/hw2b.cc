// vectorlization version
// 399.15

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define PNG_NO_SETJMP
#include <assert.h>
#include <png.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <emmintrin.h>
#include <mpi.h>
#include <omp.h>

inline void set_color(png_bytep color, int p, int iters) {
  if (p != iters) {
    if (p & 16) {
      color[0] = 240;
      color[1] = color[2] = (p & 15) * 16;
    } else {
      color[0] = (p & 15) * 16;
      color[1] = color[2] = 0;
    }
  } else {
		color[0] = color[1] = color[2] = 0;
	}
}

void write_png(const char* filename, png_bytep* rows, int width, int height, int* mapping) {
  FILE* fp = fopen(filename, "wb");
  assert(fp);
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  assert(png_ptr);
  png_infop info_ptr = png_create_info_struct(png_ptr);
  assert(info_ptr);
  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_filter(png_ptr, 0, PNG_NO_FILTERS);
  png_write_info(png_ptr, info_ptr);
  png_set_compression_level(png_ptr, 1);

  for (int i = 0; i < height; ++i) {
    png_write_row(png_ptr, rows[mapping[i]]);
  }
  png_write_end(png_ptr, NULL);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
}

int main(int argc, char** argv) {
  // detect how many CPUs are available
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  int num_cpus = CPU_COUNT(&cpu_set);
  // printf("%d cpus available\n", CPU_COUNT(&cpu_set));

	MPI_Init(&argc, &argv);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	// printf("rank %d, size %d\n", rank, size);

  // argument parsing
  assert(argc == 9);
  const char* filename = argv[1];
  int iters = strtol(argv[2], 0, 10);
  double left = strtod(argv[3], 0);
  double right = strtod(argv[4], 0);
  double lower = strtod(argv[5], 0);
  double upper = strtod(argv[6], 0);
  int width = strtol(argv[7], 0, 10);
  int height = strtol(argv[8], 0, 10);
	// printf("%d %lf %lf %lf %lf\n", iters, left, right, lower, upper);
	// printf("w: %d, h: %d\n", width, height);

	// prevent dupliate calculation
  double y_offset = (upper - lower) / height;
  double x_offset = (right - left) / width;

	// prepare for mpi
	int local_h = height / size + (height % size > rank);
	int cols = 3 * width;
	png_bytep _rows = (png_bytep)malloc(local_h * cols * sizeof(png_byte));
  png_bytep* rows = (png_bytep*)malloc(local_h * sizeof(png_bytep));
	for (int i = 0; i < local_h; ++i)
		rows[i] = &_rows[cols * i];

	int* all_h;
	int* displs;
	if (rank == 0) {
		// calculate every node's local_h

		all_h = (int*)malloc(size * sizeof(int));
		displs = (int*)malloc(size * sizeof(int));
		all_h[0] = local_h * cols;
		displs[0] = 0;
		for (int i = 1; i < size; ++i) {
			all_h[i] = ((height / size) + (height % size > i)) * cols;
			displs[i] = displs[i - 1] + all_h[i - 1];
		}
	}

	#pragma omp parallel num_threads(num_cpus)
	{
		#pragma omp for schedule(dynamic)
		for (int h = rank; h < height; h += size) {
			png_bytep row = rows[h / size];
			// printf("[%d]: h: %d at rows[%d]\n", rank, h, h/size);

			int local_width = 0;
			int repeats[2] = {0, 0};
			int doing[2] = {-1, -1};
			__m128d x0 = _mm_setzero_pd();
			__m128d y0 = _mm_set_pd1(h * y_offset + lower);
			__m128d x = _mm_setzero_pd();
			__m128d y = _mm_setzero_pd();
			__m128d xx = _mm_setzero_pd();
			__m128d yy = _mm_setzero_pd();
			__m128d length_squared = _mm_setzero_pd();
			while (true) {
				if (doing[0] == -1) {
					if (local_width == width) break;
					x0[0] = local_width * x_offset + left;
					x[0] = y[0] = xx[0] = yy[0] = length_squared[0] = 0;
					doing[0] = local_width;
					repeats[0] = 0;
					local_width++;
				}
				if (doing[1] == -1) {
					if (local_width == width) break;
					x0[1] = local_width * x_offset + left;
					x[1] = y[1] = xx[1] = yy[1] = length_squared[1] = 0;
					doing[1] = local_width;
					repeats[1] = 0;
					local_width++;
				}

				while (repeats[0] < iters && repeats[1] < iters && length_squared[0] < 4 && length_squared[1] < 4) {
					__m128d xy = _mm_mul_pd(x, y);

					y = _mm_add_pd(_mm_add_pd(xy, xy), y0);
					x = _mm_add_pd(_mm_sub_pd(xx, yy), x0);

					xx = _mm_mul_pd(x, x);
					yy = _mm_mul_pd(y, y);
					length_squared = _mm_add_pd(xx, yy);
					++repeats[0];
					++repeats[1];
				}

				if (!(repeats[0] < iters && length_squared[0] < 4)) {
					set_color(row + doing[0] * 3, repeats[0], iters);
					doing[0] = -1;
				}

				if (!(repeats[1] < iters && length_squared[1] < 4)) {
					set_color(row + doing[1] * 3, repeats[1], iters);
					doing[1] = -1;
				}
			}

			if (doing[0] != -1) {
				// do 0
				while (repeats[0] < iters && length_squared[0] < 4) {
					double xy = x[0] * y[0];

					y[0] = xy + xy + y0[0];
					x[0] = xx[0] - yy[0] + x0[0];

					xx[0] = x[0] * x[0];
					yy[0] = y[0] * y[0];
					length_squared[0] = xx[0] + yy[0];
					++repeats[0];
				}
				set_color(row + doing[0] * 3, repeats[0], iters);
			}
			if (doing[1] != -1) {
				// do 1
				while (repeats[1] < iters && length_squared[1] < 4) {
					double xy = x[1] * y[1];

					y[1] = xy + xy + y0[1];
					x[1] = xx[1] - yy[1] + x0[1];

					xx[1] = x[1] * x[1];
					yy[1] = y[1] * y[1];
					length_squared[1] = xx[1] + yy[1];
					++repeats[1];
				}
				set_color(row + doing[1] * 3, repeats[1], iters);
			}
		}
	}

	// printf("[%d]: done.\n", rank);
	png_bytep _all_rows;
  png_bytep* all_rows;
	int* mapping;
	if (rank == 0) {
		_all_rows = (png_bytep)malloc(height * cols * sizeof(png_byte));
		all_rows = (png_bytep*)malloc(height * sizeof(png_bytep));
		for (int i = 0; i < height; ++i)
			all_rows[i] = &_all_rows[cols * i];
		
		mapping = (int*)malloc(height * sizeof(int));
		int h = 0;
		for (int i = 0; i < size; i++) {
			for (int j = i; j < height; j += size) {
				mapping[height - j - 1] = h++;
			}
		}
	}

	// printf("[%d]: before gatherv.\n", rank);
	MPI_Gatherv(_rows, local_h * cols, MPI_UNSIGNED_CHAR, _all_rows, all_h, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
	MPI_Finalize();
	// printf("[%d]: after gatherv.\n", rank);

	if (rank == 0) {
		// draw and cleanup
		write_png(filename, all_rows, width, height, mapping);
		free(_all_rows);
	}

	free(_rows);
}
