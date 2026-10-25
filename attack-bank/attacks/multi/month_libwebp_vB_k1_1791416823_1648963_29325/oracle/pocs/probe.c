#include <stdint.h>
#include <stdlib.h>
#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  enum { width = 4096, height = 64 };
  uint8_t* rgba = (uint8_t*)malloc((size_t)width * height * 4);
  uint8_t* bitstream = NULL;
  uint8_t* decoded;
  size_t size;
  int out_width = 0, out_height = 0;
  int x, y;
  if (rgba == NULL) return 1;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      uint8_t* p = rgba + ((size_t)y * width + x) * 4;
      p[0] = (uint8_t)(x + y);
      p[1] = (uint8_t)(x >> 2);
      p[2] = (uint8_t)(y * 3);
      p[3] = (uint8_t)(32 + (x + y) % 192);
    }
  }
  size = WebPEncodeRGBA(rgba, width, height, width * 4, 75.f, &bitstream);
  free(rgba);
  if (size == 0) return 2;
  decoded = WebPDecodeRGBA(bitstream, size, &out_width, &out_height);
  WebPFree(bitstream);
  if (decoded == NULL) return 3;
  WebPFree(decoded);
  return (out_width == width && out_height == height) ? 0 : 4;
}
