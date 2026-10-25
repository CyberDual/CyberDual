#include <stdint.h>
#include <stdlib.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

/* Bottom-up 8-bit planes: row 0 is the last row in memory, rgb_stride < 0. */
int main(void) {
  enum { kWidth = 4, kHeight = 2 };
  const int row_bytes = kWidth;
  uint8_t* r = (uint8_t*)malloc((size_t)kHeight * row_bytes);
  uint8_t* g = (uint8_t*)malloc((size_t)kHeight * row_bytes);
  uint8_t* b = (uint8_t*)malloc((size_t)kHeight * row_bytes);
  uint8_t y[kWidth * kHeight];
  uint8_t u[((kWidth + 1) / 2) * ((kHeight + 1) / 2)];
  uint8_t v[((kWidth + 1) / 2) * ((kHeight + 1) / 2)];
  const SharpYuvConversionMatrix* matrix =
      SharpYuvGetConversionMatrix(kSharpYuvMatrixRec601Full);
  SharpYuvOptions options;
  int i, ok;

  if (r == NULL || g == NULL || b == NULL || matrix == NULL) return 1;
  for (i = 0; i < kHeight * row_bytes; ++i) {
    r[i] = (uint8_t)(i * 3);
    g[i] = (uint8_t)(80 + i);
    b[i] = (uint8_t)(160 + i);
  }
  if (!SharpYuvOptionsInit(matrix, &options)) return 1;

  ok = SharpYuvConvertWithOptions(
      r + (kHeight - 1) * row_bytes, g + (kHeight - 1) * row_bytes,
      b + (kHeight - 1) * row_bytes, /*rgb_step=*/1, /*rgb_stride=*/-row_bytes,
      /*rgb_bit_depth=*/8, y, kWidth, u, (kWidth + 1) / 2, v, (kWidth + 1) / 2,
      /*yuv_bit_depth=*/8, kWidth, kHeight, &options);
  free(r);
  free(g);
  free(b);
  return ok ? 0 : 2;
}
