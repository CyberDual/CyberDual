#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  enum { kWidth = 1024, kHeight = 32 };
  const size_t rgba_size = (size_t)kWidth * kHeight * 4;
  uint8_t* rgba = (uint8_t*)malloc(rgba_size);
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  size_t encoded_size;
  int width = 0;
  int height = 0;
  int x, y;
  if (rgba == NULL) return 1;
  for (y = 0; y < kHeight; ++y) {
    for (x = 0; x < kWidth; ++x) {
      const size_t i = ((size_t)y * kWidth + x) * 4;
      rgba[i + 0] = (uint8_t)(x * 13 + y * 7);
      rgba[i + 1] = (uint8_t)(x * 3 + y * 17);
      rgba[i + 2] = (uint8_t)(x * 11 + y * 5);
      rgba[i + 3] = (uint8_t)(x + y * 29);
    }
  }
  encoded_size = WebPEncodeRGBA(rgba, kWidth, kHeight, kWidth * 4, 75.f,
                               &encoded);
  free(rgba);
  if (encoded_size == 0 || encoded == NULL) return 2;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != kWidth || height != kHeight) {
    WebPFree(decoded);
    return 3;
  }
  WebPFree(decoded);
  return 0;
}
