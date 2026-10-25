// Exercise the sharp RGB -> YUV420 conversion through the libsharpyuv public
// API with a small planar image.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  const int width = 2;
  const int height = 3;
  const int rgb_bit_depth = 8;
  const int yuv_bit_depth = 8;
  const int rgb_step = 1;
  const int rgb_stride = width;              // one packed row per line
  const int y_stride = width;                // one row of Y samples
  const int uv_stride = (width + 1) / 2;     // one row of subsampled samples
  const int uv_height = (height + 1) / 2;
  const size_t num_samples = (size_t)width * height;
  uint8_t* const r = (uint8_t*)malloc(num_samples);
  uint8_t* const g = (uint8_t*)malloc(num_samples);
  uint8_t* const b = (uint8_t*)malloc(num_samples);
  uint8_t* const y = (uint8_t*)malloc((size_t)y_stride * height);
  uint8_t* const u = (uint8_t*)malloc((size_t)uv_stride * uv_height);
  uint8_t* const v = (uint8_t*)malloc((size_t)uv_stride * uv_height);
  const SharpYuvConversionMatrix* matrix =
      SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);
  SharpYuvOptions options;
  int ok;
  size_t i;

  if (r == NULL || g == NULL || b == NULL || y == NULL || u == NULL ||
      v == NULL || matrix == NULL) {
    fprintf(stderr, "allocation or matrix lookup failed\n");
    return 1;
  }
  for (i = 0; i < num_samples; ++i) {
    r[i] = (uint8_t)(i * 7 + 1);
    g[i] = (uint8_t)(i * 13 + 5);
    b[i] = (uint8_t)(i * 29 + 11);
  }
  memset(y, 0, (size_t)y_stride * height);
  memset(u, 0, (size_t)uv_stride * uv_height);
  memset(v, 0, (size_t)uv_stride * uv_height);

  if (!SharpYuvOptionsInit(matrix, &options)) {
    fprintf(stderr, "SharpYuvOptionsInit failed\n");
    return 1;
  }

  ok = SharpYuvConvertWithOptions(r, g, b, rgb_step, rgb_stride,
                                  rgb_bit_depth, y, y_stride, u, uv_stride, v,
                                  uv_stride, yuv_bit_depth, width, height,
                                  &options);
  printf("conversion %s\n", ok ? "succeeded" : "failed");
  printf("y[0..%d]=%d,%d,%d,%d,%d,%d u[0..%d]=%d,%d v[0..%d]=%d,%d\n",
         (int)(sizeof(y[0]) * y_stride * height - 1), y[0], y[1], y[2], y[3],
         y[4], y[5], (int)(sizeof(u[0]) * uv_stride * uv_height - 1), u[0],
         u[1], (int)(sizeof(v[0]) * uv_stride * uv_height - 1), v[0], v[1]);

  free(r);
  free(g);
  free(b);
  free(y);
  free(u);
  free(v);
  return ok ? 0 : 1;
}
