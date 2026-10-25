// Reproducer driving the public libsharpyuv entry point
// SharpYuvConvertWithOptions() (sharpyuv/sharpyuv.h) on a small RGB image.
// A properly initialized SharpYuvOptions converts fine, and the API rejects
// an out-of-range transfer_type, but a caller that skips
// SharpYuvOptionsInit() leaves options.yuv_matrix NULL and the conversion
// dereferences it instead of returning 0.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

#define W 16
#define H 16

static uint8_t r[W * H], g[W * H], b[W * H];
static uint8_t y[W * H], u[(W / 2) * (H / 2)], v[(W / 2) * (H / 2)];

static int ConvertWithOptions(const SharpYuvOptions* options) {
  return SharpYuvConvertWithOptions(r, g, b, /*rgb_step=*/1, /*rgb_stride=*/W,
                                    /*rgb_bit_depth=*/8, y, /*y_stride=*/W, u,
                                    /*u_stride=*/W / 2, v,
                                    /*v_stride=*/W / 2,
                                    /*yuv_bit_depth=*/8, W, H, options);
}

int main(void) {
  SharpYuvOptions options;
  int i;

  for (i = 0; i < W * H; ++i) {
    r[i] = (uint8_t)(i * 7);
    g[i] = (uint8_t)(i * 3);
    b[i] = (uint8_t)(i * 11 + 5);
  }

  // Ordinary call: options initialized through the documented helper.
  memset(&options, 0, sizeof(options));
  if (!SharpYuvOptionsInit(
          SharpYuvGetConversionMatrix(kSharpYuvMatrixRec601Limited),
          &options) ||
      !ConvertWithOptions(&options)) {
    fprintf(stderr, "failed: baseline conversion\n");
    return 1;
  }
  printf("baseline conversion ok\n");

  // An out-of-range transfer type must be rejected with a return value of 0.
  options.transfer_type = (SharpYuvTransferFunctionType)999;
  if (ConvertWithOptions(&options) != 0) {
    fprintf(stderr, "failed: invalid transfer_type accepted\n");
    return 1;
  }
  printf("invalid transfer_type rejected\n");

  // Same call, but the caller never initialized SharpYuvOptions, leaving
  // options.yuv_matrix NULL: also expected to be rejected with 0.
  memset(&options, 0, sizeof(options));
  options.transfer_type = kSharpYuvTransferFunctionSrgb;
  if (ConvertWithOptions(&options) != 0) {
    fprintf(stderr, "failed: NULL yuv_matrix accepted\n");
    return 1;
  }
  printf("NULL yuv_matrix rejected\n");
  return 0;
}
