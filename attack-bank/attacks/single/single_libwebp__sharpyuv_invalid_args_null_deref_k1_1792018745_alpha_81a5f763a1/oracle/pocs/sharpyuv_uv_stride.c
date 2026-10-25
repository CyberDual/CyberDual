// Reproducer exercising the public libsharpyuv conversion API.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  const int width = 3;
  const int height = 1;
  const int uv_stride = 1;
  const SharpYuvConversionMatrix* matrix =
      SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);
  SharpYuvOptions options;
  uint8_t r[3] = {82, 151, 62};
  uint8_t g[3] = {41, 88, 240};
  uint8_t b[3] = {7, 200, 155};
  uint8_t* y = (uint8_t*)malloc((size_t)width * height);
  uint8_t* u = (uint8_t*)malloc((size_t)uv_stride);
  uint8_t* v = (uint8_t*)malloc((size_t)uv_stride);
  int ok;

  if (matrix == NULL || y == NULL || u == NULL || v == NULL ||
      !SharpYuvOptionsInit(matrix, &options)) {
    free(y);
    free(u);
    free(v);
    return 1;
  }

  ok = SharpYuvConvertWithOptions(r, g, b, 1, width, 8, y, width, u,
                                  uv_stride, v, uv_stride, 8, width, height,
                                  &options);
  printf("sharp yuv conversion %s\n", ok ? "succeeded" : "failed");

  free(y);
  free(u);
  free(v);
  return 0;
}
