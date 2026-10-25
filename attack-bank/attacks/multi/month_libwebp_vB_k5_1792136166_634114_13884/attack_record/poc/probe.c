#include <stdint.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

/* Wide lossless image. The gradient is in-contract input for WebPEncode
   (dimensions within WEBP_MAX_DIMENSION, opaque RGBA) and for WebPDecodeRGBA. */
enum { kWidth = 12944, kHeight = 110 };

int main(void) {
  const size_t n = (size_t)kWidth * (size_t)kHeight;
  uint8_t* rgba = (uint8_t*)malloc(n * 4);
  WebPPicture pic;
  WebPConfig config;
  WebPMemoryWriter writer;
  uint8_t* out;
  int w = 0;
  int h = 0;
  int y, x;
  if (rgba == NULL) return 1;
  for (y = 0; y < kHeight; ++y) {
    for (x = 0; x < kWidth; ++x) {
      uint8_t* p = rgba + ((size_t)y * kWidth + (size_t)x) * 4;
      p[0] = (uint8_t)(x * 3 + y);
      p[1] = (uint8_t)(y * 5 + x);
      p[2] = (uint8_t)(x + y * 2);
      p[3] = 255;
    }
  }
  if (!WebPPictureInit(&pic)) return 2;
  pic.width = kWidth;
  pic.height = kHeight;
  pic.use_argb = 1;
  if (!WebPPictureImportRGBA(&pic, rgba, kWidth * 4)) return 3;
  if (!WebPConfigInit(&config)) return 4;
  config.lossless = 1;
  config.quality = 0;
  config.method = 0;
  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;
  if (!WebPEncode(&config, &pic)) return 5;
  out = WebPDecodeRGBA(writer.mem, writer.size, &w, &h);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&pic);
  free(rgba);
  if (out == NULL) return 6;
  WebPFree(out);
  return 0;
}
