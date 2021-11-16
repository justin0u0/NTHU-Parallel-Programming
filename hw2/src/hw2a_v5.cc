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

// Global share variables
int iters;
double left;
double right;
double lower;
double upper;
int width;
int height;
int num_cpus;
png_bytep* rows;
int cur_height;
pthread_mutex_t mx;
double y_offset;
double x_offset;

inline void set_color(png_bytep color, int p) {
  if (p != iters) {
    if (p & 16) {
      color[0] = 240;
      color[1] = color[2] = (p & 15) * 16;
    } else {
      color[0] = (p & 15) * 16;
      color[1] = color[2] = 0;
    }
  }
}

void* mandelbrot(void* arg) {
  int tid = *((int*)arg);

  while (true) {
    int local_height;
    pthread_mutex_lock(&mx);
    local_height = cur_height++;
    pthread_mutex_unlock(&mx);

    if (local_height >= height) break;
    size_t row_size = 3 * width * sizeof(png_byte);
    png_bytep row = (png_bytep)malloc(row_size);
    rows[height - local_height - 1] = row;

    int local_width = 0;
    int widths[2] = {0, 0};
    int repeats[2] = {0, 0};
    int doing[2] = {-1, -1};
    __m128d x0 = _mm_setzero_pd();
    __m128d y0 = _mm_set_pd1(local_height * y_offset + lower);
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
        set_color(row + doing[0] * 3, repeats[0]);
        doing[0] = -1;
      }

      if (!(repeats[1] < iters && length_squared[1] < 4)) {
        set_color(row + doing[1] * 3, repeats[1]);
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
      set_color(row + doing[0] * 3, repeats[0]);
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
      set_color(row + doing[1] * 3, repeats[1]);
    }
  }

  return 0;
}

void write_png(const char* filename) {
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
    png_write_row(png_ptr, rows[i]);
    free(rows[i]);
  }
  png_write_end(png_ptr, NULL);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
}

int main(int argc, char** argv) {
  // detect how many CPUs are available
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  num_cpus = CPU_COUNT(&cpu_set);
  // printf("%d cpus available\n", CPU_COUNT(&cpu_set));

  // argument parsing
  assert(argc == 9);
  const char* filename = argv[1];
  iters = strtol(argv[2], 0, 10);
  left = strtod(argv[3], 0);
  right = strtod(argv[4], 0);
  lower = strtod(argv[5], 0);
  upper = strtod(argv[6], 0);
  width = strtol(argv[7], 0, 10);
  height = strtol(argv[8], 0, 10);

  // initialize shared variables
  cur_height = 0;
  y_offset = (upper - lower) / height;
  x_offset = (right - left) / width;

  pthread_mutex_init(&mx, 0);

  pthread_t* threads = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
  int* ids = (int*)malloc(num_cpus * sizeof(int));
  rows = (png_bytep*)malloc(height * sizeof(png_bytep));
  for (int i = 0; i < num_cpus; i++) {
    ids[i] = i;
    pthread_create(&threads[i], 0, mandelbrot, (void*)&ids[i]);
  }

  for (int i = 0; i < num_cpus; i++) {
    pthread_join(threads[i], 0);
  }

  // draw and cleanup
  write_png(filename);

  pthread_mutex_destroy(&mx);
}
