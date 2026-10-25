#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  const int width = 1;
  const int height = 3;
  const int rgb_step = 1;
  const int rgb_stride = 1 << 30;
  const int rgb_bit_depth = 8;
  const int yuv_bit_depth = 8;
  const size_t span = 2 * (size_t)rgb_stride + 64;
  uint8_t *r, *g, *b;
  uint8_t y[8], u[8], v[8];
  const SharpYuvConversionMatrix* matrix;
  SharpYuvOptions options;
  int rc;

  r = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
           0);
  g = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
           0);
  b = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
           0);
  if (r == MAP_FAILED || g == MAP_FAILED || b == MAP_FAILED) {
    perror("mmap");
    return 2;
  }

  r[0] = 10;
  g[0] = 20;
  b[0] = 30;
  r[(size_t)rgb_stride] = 40;
  g[(size_t)rgb_stride] = 50;
  b[(size_t)rgb_stride] = 60;
  r[2 * (size_t)rgb_stride] = 70;
  g[2 * (size_t)rgb_stride] = 80;
  b[2 * (size_t)rgb_stride] = 90;

  memset(y, 0, sizeof(y));
  memset(u, 0, sizeof(u));
  memset(v, 0, sizeof(v));

  matrix = SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);
  if (matrix == NULL || !SharpYuvOptionsInit(matrix, &options)) {
    fprintf(stderr, "options init failed\n");
    return 2;
  }

  rc = SharpYuvConvertWithOptions(r, g, b, rgb_step, rgb_stride, rgb_bit_depth,
                                  y, width, u, 1, v, 1, yuv_bit_depth, width,
                                  height, &options);
  printf("rc=%d\n", rc);
  return rc ? 0 : 1;
}
