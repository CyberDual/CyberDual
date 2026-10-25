#include <stdint.h>
#include <stdlib.h>
#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  const int width = 1024, height = 128;
  uint8_t* const rgba = (uint8_t*)malloc((size_t)width * height * 4);
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  size_t size;
  int x, y, out_width, out_height;
  if (rgba == NULL) return 1;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      uint8_t* const p = rgba + 4 * ((size_t)y * width + x);
      p[0] = (uint8_t)(x * 17 + y * 3);
      p[1] = (uint8_t)(x * 7 + y * 11);
      p[2] = (uint8_t)(x + y * 13);
      p[3] = (uint8_t)(x * 19 + y * 23);
    }
  }
  size = WebPEncodeRGBA(rgba, width, height, width * 4, 75.f, &encoded);
  free(rgba);
  if (size == 0) return 2;
  decoded = WebPDecodeRGBA(encoded, size, &out_width, &out_height);
  WebPFree(encoded);
  if (decoded == NULL) return 3;
  WebPFree(decoded);
  return out_width == width && out_height == height ? 0 : 4;
}
