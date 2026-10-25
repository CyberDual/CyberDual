#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int width = 128;
  const int height = 32;
  const size_t pixel_count = (size_t)width * height;
  uint8_t* const input = (uint8_t*)malloc(pixel_count * 4);
  uint8_t* encoded = NULL;
  uint8_t* decoded = NULL;
  int decoded_width = 0;
  int decoded_height = 0;
  size_t encoded_size;

  if (input == NULL) return 1;
  for (size_t i = 0; i < pixel_count; ++i) {
    uint8_t* const pixel = &input[i * 4];
    pixel[0] = (uint8_t)i;
    pixel[1] = (uint8_t)(i >> 8);
    pixel[2] = (uint8_t)(i >> 16);
    pixel[3] = (uint8_t)(i + (i >> 7));
  }

  encoded_size =
      WebPEncodeRGBA(input, width, height, width * 4, 75.0f, &encoded);

  if (encoded_size == 0) {
    free(input);
    return 1;
  }
  decoded = WebPDecodeRGBA(encoded, encoded_size, &decoded_width,
                            &decoded_height);
  free(input);
  WebPFree(encoded);
  WebPFree(decoded);
  return decoded == NULL || decoded_width != width || decoded_height != height;
}
