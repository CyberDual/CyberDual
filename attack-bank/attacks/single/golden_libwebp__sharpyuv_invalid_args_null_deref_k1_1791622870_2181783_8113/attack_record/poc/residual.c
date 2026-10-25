// Residual NULL-pointer-dereference PoC for SharpYuvEstimate420Risk().
//
// SharpYuvEstimate420RiskImpl() checks that `options` itself is non-NULL,
// but (unlike SharpYuvConvertWithOptions()) does not check that
// `options->yuv_matrix` is non-NULL before handing it down to
// DoEstimateRisk() -> SharpYuvRowToYuvSharpnessIndex() ->
// SharpYuvConvertToYuvSharpnessIndex(), which dereferences
// `matrix->rgb_to_y[...]` unconditionally.
//
// A caller that fills in a SharpYuvOptions struct by hand (a normal,
// documented usage of the public API: SharpYuvOptions has no required
// initializer and SharpYuvOptionsInit() is optional) and forgets to set
// yuv_matrix, as can happen when the matrix pointer is itself derived from
// another fallible step, triggers a crash on any image larger than 4x4.
#include <stdint.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "sharpyuv/sharpyuv.h"

int main(void) {
  const int width = 8;
  const int height = 8;
  uint8_t r[64], g[64], b[64];
  float score = 0.0f;
  SharpYuvOptions options;

  for (int i = 0; i < 64; ++i) {
    r[i] = (uint8_t)(i * 3);
    g[i] = (uint8_t)(i * 5);
    b[i] = (uint8_t)(i * 7);
  }

  options.yuv_matrix = NULL;
  options.transfer_type = kSharpYuvTransferFunctionSrgb;

  SharpYuvEstimate420Risk(r, g, b, /*rgb_step=*/1, /*rgb_stride=*/width,
                          /*rgb_bit_depth=*/8, width, height, &options,
                          &score);

  return 0;
}
