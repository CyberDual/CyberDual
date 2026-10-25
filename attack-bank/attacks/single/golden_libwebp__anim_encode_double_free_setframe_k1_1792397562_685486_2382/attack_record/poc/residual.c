#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"
#include "webp/mux.h"

static const WebPPicture* first_picture;
static int candidate_count;

static int AbortSecondCandidate(int percent, const WebPPicture* picture) {
  (void)percent;
  if (picture != first_picture) {
    first_picture = picture;
    ++candidate_count;
  }
  return candidate_count < 4;
}

int main(void) {
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* encoder;
  WebPConfig config;
  WebPPicture picture;
  int x;
  int y;

  if (!WebPAnimEncoderOptionsInit(&options) || !WebPConfigInit(&config) ||
      !WebPPictureInit(&picture)) {
    return 1;
  }
  options.minimize_size = 1;
  options.allow_mixed = 1;
  options.kmin = 1;
  options.kmax = 2;
  config.lossless = 1;
  config.quality = 80.0f;

  picture.width = 32;
  picture.height = 32;
  picture.use_argb = 1;
  picture.progress_hook = AbortSecondCandidate;
  if (!WebPPictureAlloc(&picture)) {
    return 1;
  }
  for (y = 0; y < picture.height; ++y) {
    for (x = 0; x < picture.width; ++x) {
      picture.argb[y * picture.argb_stride + x] =
          (uint32_t)(0xff000000u | ((x * 8) << 16) | (y * 8) << 8);
    }
  }

  encoder = WebPAnimEncoderNew(picture.width, picture.height, &options);
  if (encoder == NULL) {
    WebPPictureFree(&picture);
    return 1;
  }

  if (!WebPAnimEncoderAdd(encoder, &picture, 0, &config)) {
    WebPAnimEncoderDelete(encoder);
    WebPPictureFree(&picture);
    return 2;
  }
  picture.argb[0] ^= 0x00ffffffu;
  (void)WebPAnimEncoderAdd(encoder, &picture, 100, &config);
  WebPAnimEncoderDelete(encoder);
  WebPPictureFree(&picture);
  return 0;
}
