#include <stdint.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int width = 4096;
  const int height = 128;
  const size_t count = (size_t)width * height;
  uint8_t* const rgba = (uint8_t*)malloc(4 * count);
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  size_t encoded_size;
  int decoded_width = 0, decoded_height = 0;
  size_t i;
  if (rgba == NULL) return 1;
  for (i = 0; i < count; ++i) {
    rgba[4 * i + 0] = (uint8_t)(i * 13);
    rgba[4 * i + 1] = (uint8_t)(i * 7);
    rgba[4 * i + 2] = (uint8_t)(i * 3);
    rgba[4 * i + 3] = (uint8_t)(i / width);
  }
  encoded_size = WebPEncodeRGBA(rgba, width, height, 4 * width, 75.f, &encoded);
  if (encoded_size == 0) {
    free(rgba);
    return 2;
  }
  decoded = WebPDecodeRGBA(encoded, encoded_size, &decoded_width,
                          &decoded_height);
  if (decoded == NULL || decoded_width != width || decoded_height != height) {
    WebPFree(encoded);
    free(rgba);
    return 3;
  }
  WebPFree(decoded);
  WebPFree(encoded);
  free(rgba);
  return 0;
}
