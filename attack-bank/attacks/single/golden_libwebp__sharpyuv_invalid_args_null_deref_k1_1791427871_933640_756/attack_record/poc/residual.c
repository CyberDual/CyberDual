#include <stdint.h>
#include <stdio.h>

#include "src/webp/decode.h"
#include "src/webp/encode.h"

int main(void) {
  enum { kWidth = 5, kHeight = 4 };
  uint8_t rgba[kWidth * kHeight * 4];
  uint8_t* source_webp = NULL;
  uint8_t* decoded;
  uint8_t* roundtrip;
  size_t source_size;
  int width, height;
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  int i;

  for (i = 0; i < kWidth * kHeight; ++i) {
    rgba[4 * i + 0] = (uint8_t)(17 * i);
    rgba[4 * i + 1] = (uint8_t)(37 * i);
    rgba[4 * i + 2] = (uint8_t)(53 * i);
    rgba[4 * i + 3] = 255;
  }
  source_size = WebPEncodeLosslessRGBA(rgba, kWidth, kHeight, 4 * kWidth,
                                       &source_webp);
  if (source_size == 0) return 1;

  decoded = WebPDecodeRGBA(source_webp, source_size, &width, &height);
  WebPFree(source_webp);
  if (decoded == NULL || width != kWidth || height != kHeight) return 2;

  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) return 3;
  picture.width = width;
  picture.height = height;
  picture.use_argb = 1;
  if (!WebPPictureImportRGBA(&picture, decoded, 4 * width)) return 4;
  WebPFree(decoded);

  config.lossless = 0;
  config.use_sharp_yuv = 1;
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(&config, &picture)) return 5;
  WebPPictureFree(&picture);

  roundtrip = WebPDecodeRGBA(writer.mem, writer.size, &width, &height);
  WebPMemoryWriterClear(&writer);
  if (roundtrip == NULL || width != kWidth || height != kHeight) return 6;
  WebPFree(roundtrip);
  puts("round trip completed");
  return 0;
}
