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
int* image;
int num_cpus;
png_bytep* rows;

void* mandelbrot(void* arg) {
  int tid = *((int*)arg);

  int low_j = tid * (height / num_cpus);
  int high_j = (tid == num_cpus - 1) ? height : (tid + 1) * (height / num_cpus);

  int* image_ptr = image + low_j * width;

  for (int j = low_j; j < high_j; ++j) {
    double y0 = j * ((upper - lower) / height) + lower;

    for (int i = 0; i < width; ++i) {
      double x0 = i * ((right - left) / width) + left;

      int repeats = 0;
      double x = 0;
      double y = 0;
      double length_squared = 0;
      while (repeats < iters && length_squared < 4) {
        double temp = x * x - y * y + x0;
        y = 2 * x * y + y0;
        x = temp;
        length_squared = x * x + y * y;
        ++repeats;
      }
      *image_ptr++ = repeats;
    }
  }

  size_t row_size = 3 * width * sizeof(png_byte);
  for (int y = low_j; y < high_j; ++y) {
    png_bytep row = (png_bytep)malloc(row_size);
    memset(row, 0, row_size);

    for (int x = 0; x < width; ++x) {
      int p = image[y * width + x];
      png_bytep color = row + x * 3;
      if (p != iters) {
        if (p & 16) {
          color[0] = 240;
          color[1] = color[2] = p % 16 * 16;
        } else {
          color[0] = p % 16 * 16;
        }
      }
    }
    rows[(height - 1 - y)] = row;
  }

  return 0;
}

void write_png(const char* filename, const int* buffer) {
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
  for (int y = 0; y < height; ++y) {
    png_write_row(png_ptr, rows[y]);
    free(rows[y]);
  }
  png_write_end(png_ptr, NULL);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
}

int main(int argc, char** argv) {
  /* detect how many CPUs are available */
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  num_cpus = CPU_COUNT(&cpu_set);
  // printf("%d cpus available\n", CPU_COUNT(&cpu_set));

  /* argument parsing */
  assert(argc == 9);
  const char* filename = argv[1];
  iters = strtol(argv[2], 0, 10);
  left = strtod(argv[3], 0);
  right = strtod(argv[4], 0);
  lower = strtod(argv[5], 0);
  upper = strtod(argv[6], 0);
  width = strtol(argv[7], 0, 10);
  height = strtol(argv[8], 0, 10);

  /* allocate memory for image */
  image = (int*)malloc(width * height * sizeof(int));
  assert(image);

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
  write_png(filename, image);
  free(image);
}
