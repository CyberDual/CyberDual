#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

/* 1024x32 lossy WebP with an uncompressed alpha plane. The image is a
 * valid, in-range encode of ordinary RGBA pixels. */
int main(void) {
  enum { W = 1024, H = 32 };
  uint8_t* rgba = (uint8_t*)malloc((size_t)W * H * 4);
  WebPConfig config;
  WebPPicture pic;
  WebPMemoryWriter writer;
  uint8_t* out;
  int out_w = 0, out_h = 0;
  int y, x;

  if (rgba == NULL) return 2;
  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      uint8_t* p = rgba + ((size_t)y * W + x) * 4;
      p[0] = (uint8_t)(x * 3 + y);
      p[1] = (uint8_t)(y * 5 + 17);
      p[2] = (uint8_t)(x ^ y);
      p[3] = 200;
    }
  }
  /* Keep the ALPH chunk: a fully transparent pixel and a partial one. */
  rgba[3] = 0;
  rgba[7] = 128;

  if (!WebPConfigInit(&config) || !WebPPictureInit(&pic)) return 3;
  config.lossless = 0;
  config.quality = 40;
  config.method = 0;
  config.filter_strength = 50;
  config.filter_sharpness = 0;
  config.filter_type = 1;
  config.autofilter = 0;
  config.alpha_compression = 0;
  config.alpha_filtering = 0;
  config.alpha_quality = 100;
  config.thread_level = 0;
  config.pass = 1;
  config.segments = 1;
  config.sns_strength = 0;
  config.exact = 1;
  if (!WebPValidateConfig(&config)) return 4;

  pic.width = W;
  pic.height = H;
  pic.use_argb = 0;
  if (!WebPPictureImportRGBA(&pic, rgba, W * 4)) return 5;

  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;
  if (!WebPEncode(&config, &pic)) {
    WebPPictureFree(&pic);
    free(rgba);
    return 6;
  }

  out = WebPDecodeRGBA(writer.mem, writer.size, &out_w, &out_h);
  WebPFree(out);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&pic);
  free(rgba);
  return out != NULL ? 0 : 7;
}
