#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int w = 32;
  const int h = 40;
  uint8_t* rgba;
  WebPConfig config;
  WebPPicture pic;
  WebPMemoryWriter writer;
  uint8_t* out;
  int out_w = 0;
  int out_h = 0;
  int i;

  rgba = (uint8_t*)malloc((size_t)w * h * 4);
  if (rgba == NULL) return 1;
  for (i = 0; i < w * h; ++i) {
    rgba[4 * i + 0] = (uint8_t)(i * 3);
    rgba[4 * i + 1] = (uint8_t)(i * 5);
    rgba[4 * i + 2] = (uint8_t)(80 + (i & 63));
    rgba[4 * i + 3] = (uint8_t)(100 + (i & 31));
  }

  if (!WebPConfigInit(&config) || !WebPPictureInit(&pic)) return 1;
  config.lossless = 0;
  config.quality = 60;
  config.method = 0;
  config.alpha_compression = 0;
  config.alpha_quality = 100;
  config.exact = 1;
  config.filter_strength = 0;
  if (!WebPValidateConfig(&config)) return 1;

  pic.use_argb = 1;
  pic.width = w;
  pic.height = h;
  if (!WebPPictureImportRGBA(&pic, rgba, w * 4)) return 1;

  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;
  if (!WebPEncode(&config, &pic)) {
    fprintf(stderr, "encode failed %d\n", pic.error_code);
    return 1;
  }

  out = WebPDecodeRGBA(writer.mem, writer.size, &out_w, &out_h);
  if (out != NULL) WebPFree(out);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&pic);
  free(rgba);
  return out == NULL ? 2 : 0;
}
