// Exercises the public SharpYuvEstimate420Risk() API from extras/ with a
// small RGB image and a crafted SharpYuvOptions whose yuv_matrix is unset.
//
// Prints "POC_START" before invoking the library and the result line after
// the call returns, so a harness can distinguish a crash inside the library
// call (started, no result line) from unrelated process startup failures.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  const int width = 16;
  const int height = 16;
  uint8_t r[16 * 16], g[16 * 16], b[16 * 16];
  SharpYuvOptions options;
  float score = -1.f;
  int ok;
  int i;

  for (i = 0; i < width * height; ++i) {
    r[i] = (uint8_t)(3 * i);
    g[i] = (uint8_t)(255 - 2 * i);
    b[i] = (uint8_t)(17 + i);
  }
  if (!SharpYuvOptionsInit(SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp),
                           &options)) {
    fprintf(stderr, "options init failed\n");
    return 2;
  }
  options.yuv_matrix = NULL;
  printf("POC_START\n");
  fflush(stdout);
  ok = SharpYuvEstimate420Risk(r, g, b, /*rgb_step=*/1, /*rgb_stride=*/width,
                               /*rgb_bit_depth=*/8, width, height, &options,
                               &score);
  printf("POC_RESULT ok=%d score=%f\n", ok, (double)score);
  fflush(stdout);
  return 0;
}
