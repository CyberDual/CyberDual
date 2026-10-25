#include <stdint.h>
#include <stdio.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/mux.h"

static int StopEncoding(int percent, const WebPPicture* picture) {
  (void)percent;
  (void)picture;
  return 0;
}

int main(void) {
  enum { kWidth = 32, kHeight = 32 };
  uint8_t rgba[kWidth * kHeight * 4];
  uint8_t* encoded = NULL;
  uint8_t* decoded = NULL;
  size_t encoded_size;
  int width, height;
  WebPPicture picture;
  WebPConfig config;
  WebPAnimEncoder* encoder;
  int result;
  int x, y;

  for (y = 0; y < kHeight; ++y) {
    for (x = 0; x < kWidth; ++x) {
      uint8_t* const pixel = &rgba[4 * (y * kWidth + x)];
      pixel[0] = (uint8_t)(x * 7);
      pixel[1] = (uint8_t)(y * 5);
      pixel[2] = (uint8_t)(x ^ y);
      pixel[3] = 255;
    }
  }
  encoded_size = WebPEncodeLosslessRGBA(rgba, kWidth, kHeight, kWidth * 4,
                                       &encoded);
  if (encoded_size == 0) return 2;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != kWidth || height != kHeight) return 3;

  if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) return 4;
  picture.use_argb = 1;
  picture.width = width;
  picture.height = height;
  if (!WebPPictureImportRGBA(&picture, decoded, width * 4)) return 5;
  WebPFree(decoded);
  picture.progress_hook = StopEncoding;
  config.lossless = 1;
  encoder = WebPAnimEncoderNew(width, height, NULL);
  if (encoder == NULL) return 6;
  result = WebPAnimEncoderAdd(encoder, &picture, 0, &config);
  WebPAnimEncoderDelete(encoder);
  WebPPictureFree(&picture);
  if (result != 0) {
    fputs("progress callback was not used\n", stderr);
    return 7;
  }
  return 0;
}
