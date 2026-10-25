#include <stdint.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  enum { kWidth = 128, kHeight = 128 };
  uint8_t pixels[kWidth * kHeight * 4];
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  uint32_t state = 1;
  size_t encoded_size;
  int width, height;
  int i;

  for (i = 0; i < kWidth * kHeight; ++i) {
    const uint32_t color = (state = state * 1664525u + 1013904223u) >> 20;
    pixels[4 * i + 0] = (uint8_t)(color * 13u);
    pixels[4 * i + 1] = (uint8_t)(color >> 2);
    pixels[4 * i + 2] = (uint8_t)(color * 37u);
    pixels[4 * i + 3] = 255;
  }

  encoded_size = WebPEncodeLosslessRGBA(pixels, kWidth, kHeight, 4 * kWidth,
                                        &encoded);
  if (encoded_size == 0) return 1;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL) return 2;
  WebPFree(decoded);
  return (width != kWidth || height != kHeight);
}
