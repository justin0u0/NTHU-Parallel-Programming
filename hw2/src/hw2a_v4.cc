// pthread normal version
// minor optimization includes:
// - use &15 instead of %16
// - no need to memset row
// - prevent duplicate calculation using y_offset, x_offset, xx, xy, yy
// 674.45

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

void* mandelbrot(void* arg) {
  int tid = *((int*)arg);

  size_t row_size = 3 * width * sizeof(png_byte);

  while (true) {
    int local_height;
    pthread_mutex_lock(&mx);
    local_height = cur_height++;
    pthread_mutex_unlock(&mx);

    if (local_height >= height) break;

    double y0 = local_height * y_offset + lower;

    png_bytep row = (png_bytep)malloc(row_size);
    // memset(row, 0, row_size);

    for (int i = 0; i < width; ++i) {
      double x0 = i * x_offset + left;

      int repeats = 0;
      double x = 0;
      double y = 0;
      double length_squared = 0;
      double xx = 0;
      double yy = 0;
      while (repeats < iters && length_squared < 4) {
        double xy = x * y;

        y = xy + xy + y0;
        x = xx - yy + x0;

        xx = x * x;
        yy = y * y;
        length_squared = xx + yy;
        ++repeats;
      }

      // write image row
      int p = repeats;
      png_bytep color = row + i * 3;
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
    rows[height - 1 - local_height] = row;
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

  /* draw and cleanup */
  write_png(filename);

  pthread_mutex_destroy(&mx);
}
