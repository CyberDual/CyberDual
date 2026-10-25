// Converts a small RGB frame to YUV 4:2:0 through the libsharpyuv public
// API, using a caller supplied conversion matrix whose gains are scaled for
// a higher sample bit depth.
#include <stdint.h>
#include <stdio.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  enum { kWidth = 8, kHeight = 8 };
  enum { kUVSize = ((kWidth + 1) / 2) * ((kHeight + 1) / 2) };
  uint8_t r[kWidth * kHeight], g[kWidth * kHeight], b[kWidth * kHeight];
  uint8_t y[kWidth * kHeight];
  uint8_t u[kUVSize], v[kUVSize];
  SharpYuvConversionMatrix matrix =
      *SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);
  SharpYuvOptions options;
  int i, ok;

  // Scale the gains the way SharpYuvComputeConversionMatrix() would for a
  // higher sample bit depth.
  for (i = 0; i < 3; ++i) {
    matrix.rgb_to_y[i] *= 16;
    matrix.rgb_to_u[i] *= 16;
    matrix.rgb_to_v[i] *= 16;
  }
  if (!SharpYuvOptionsInit(&matrix, &options)) {
    fprintf(stderr, "SharpYuvOptionsInit failed\n");
    return 1;
  }
  for (i = 0; i < kWidth * kHeight; ++i) {
    r[i] = (uint8_t)(i * 3);
    g[i] = (uint8_t)(255 - 2 * i);
    b[i] = (uint8_t)(i * 5);
  }
  ok = SharpYuvConvertWithOptions(r, g, b, /*rgb_step=*/1, /*rgb_stride=*/kWidth,
                                  /*rgb_bit_depth=*/8, y, /*y_stride=*/kWidth, u,
                                  /*u_stride=*/(kWidth + 1) / 2, v,
                                  /*v_stride=*/(kWidth + 1) / 2,
                                  /*yuv_bit_depth=*/8, kWidth, kHeight,
                                  &options);
  printf("SharpYuvConvertWithOptions -> %d\n", ok);
  return ok ? 0 : 1;
}
