#include <stdint.h>
#include <stdlib.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"
#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  enum { width = 8, height = 8 };
  uint8_t rgba[width * height * 4];
  uint8_t *webp = NULL, *decoded;
  uint16_t y[width * height], u[(width / 2) * (height / 2)];
  uint16_t v[(width / 2) * (height / 2)];
  SharpYuvConversionMatrix matrix =
      *SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);
  SharpYuvOptions options;
  int decoded_width, decoded_height;
  size_t webp_size;
  int i;

  for (i = 0; i < width * height; ++i) {
    rgba[4 * i + 0] = (uint8_t)(i * 3);
    rgba[4 * i + 1] = (uint8_t)(i * 5);
    rgba[4 * i + 2] = (uint8_t)(i * 7);
    rgba[4 * i + 3] = 255;
  }
  webp_size = WebPEncodeRGBA(rgba, width, height, 4 * width, 75.f, &webp);
  if (webp_size == 0) return 2;
  decoded = WebPDecodeRGBA(webp, webp_size, &decoded_width, &decoded_height);
  WebPFree(webp);
  if (decoded == NULL || decoded_width != width || decoded_height != height) {
    WebPFree(decoded);
    return 3;
  }
  if (!SharpYuvOptionsInit(&matrix, &options)) return 4;
  i = SharpYuvConvertWithOptions(
      decoded + 0, decoded + 1, decoded + 2, 4, 4 * width, 8, y,
      2 * width, u, width, v, width, 10, width, height, &options);
  WebPFree(decoded);
  return i ? 0 : 5;
}
