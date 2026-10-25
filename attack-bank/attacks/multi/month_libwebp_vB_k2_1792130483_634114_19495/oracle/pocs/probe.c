#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define W 45
#define H 160

int main(void) {
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  uint8_t* rgba;
  uint8_t* out;
  int width = 0, height = 0;
  int y, x;

  if (!WebPConfigInit(&config)) return 1;
  config.lossless = 0;
  config.quality = 20;
  config.method = 0;
  config.filter_strength = 40;
  config.filter_type = 1; /* strong */
  config.alpha_compression = 0;
  config.alpha_filtering = 0;
  config.alpha_quality = 100;
  config.thread_level = 0;
  config.segments = 1;
  config.pass = 1;
  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "config invalid\n");
    return 2;
  }

  rgba = (uint8_t*)malloc((size_t)W * H * 4);
  if (rgba == NULL) return 3;
  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      uint8_t* p = rgba + ((size_t)y * W + x) * 4;
      p[0] = (uint8_t)(x * 5);
      p[1] = (uint8_t)(y * 3);
      p[2] = (uint8_t)(x + y);
      p[3] = (uint8_t)((x + y) & 1 ? 255 : 128);
    }
  }

  if (!WebPPictureInit(&picture)) return 4;
  picture.width = W;
  picture.height = H;
  picture.use_argb = 1;
  if (!WebPPictureImportRGBA(&picture, rgba, W * 4)) {
    fprintf(stderr, "import failed\n");
    return 5;
  }
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "encode failed %d\n", picture.error_code);
    return 6;
  }
  fprintf(stderr, "encoded %zu bytes\n", writer.size);

  out = WebPDecodeRGBA(writer.mem, writer.size, &width, &height);
  fprintf(stderr, "decode %p %dx%d\n", (void*)out, width, height);
  WebPFree(out);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&picture);
  free(rgba);
  return 0;
}
