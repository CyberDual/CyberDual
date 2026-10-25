#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int width = 256;
  const int height = 256;
  uint8_t* rgba = (uint8_t*)malloc((size_t)width * height * 4);
  uint8_t* encoded = NULL;
  size_t encoded_size;
  int x;
  int y;
  if (rgba == NULL) return 2;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      uint8_t* pixel = &rgba[(size_t)y * width * 4 + (size_t)x * 4];
      pixel[0] = (uint8_t)x;
      pixel[1] = (uint8_t)y;
      pixel[2] = (uint8_t)(x ^ y);
      pixel[3] = (uint8_t)((x * 7 + y * 13) & 0xff);
    }
  }
  encoded_size =
      WebPEncodeRGBA(rgba, width, height, width * 4, 80.0f, &encoded);
  free(rgba);
  if (encoded_size == 0) return 3;
  {
    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config)) return 4;
    config.output.colorspace = MODE_RGBA;
    config.options.alpha_dithering_strength = 0;
    if (WebPDecode(encoded, encoded_size, &config) != VP8_STATUS_OK) {
      WebPFreeDecBuffer(&config.output);
      WebPFree(encoded);
      return 5;
    }
    WebPFreeDecBuffer(&config.output);
  }
  WebPFree(encoded);
  return 0;
}
