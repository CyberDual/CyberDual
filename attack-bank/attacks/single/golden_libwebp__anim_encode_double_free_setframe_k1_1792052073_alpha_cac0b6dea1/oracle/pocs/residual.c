// Abort the second candidate encode via the public progress hook.
#include <stdio.h>
#include <stdlib.h>

#include "src/webp/encode.h"
#include "src/webp/mux.h"

static int g_seen_done = 0;

static int AbortSecondCandidate(int percent, const WebPPicture* picture) {
  (void)picture;
  if (g_seen_done) return 0;
  if (percent >= 100) g_seen_done = 1;
  return 1;
}

static int FillFrame(WebPPicture* pic, int width, int height, uint8_t seed) {
  int y, x;
  if (!WebPPictureInit(pic)) return 0;
  pic->use_argb = 1;
  pic->width = width;
  pic->height = height;
  if (!WebPPictureAlloc(pic)) return 0;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      const uint8_t r = (uint8_t)(seed + x * 3);
      const uint8_t g = (uint8_t)(y * 5);
      const uint8_t b = (uint8_t)(x + y + seed);
      pic->argb[y * pic->argb_stride + x] =
          (0xffu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
  }
  pic->progress_hook = AbortSecondCandidate;
  return 1;
}

int main(void) {
  const int width = 32;
  const int height = 32;
  WebPAnimEncoderOptions enc_options;
  WebPConfig config;
  WebPAnimEncoder* enc;
  WebPPicture frame;
  WebPData webp;

  if (!WebPAnimEncoderOptionsInit(&enc_options) || !WebPConfigInit(&config)) {
    fprintf(stderr, "init failed\n");
    return 1;
  }
  enc_options.allow_mixed = 1;
  enc_options.minimize_size = 1;
  config.lossless = 1;

  enc = WebPAnimEncoderNew(width, height, &enc_options);
  if (enc == NULL) {
    fprintf(stderr, "encoder alloc failed\n");
    return 1;
  }
  if (!FillFrame(&frame, width, height, 20)) {
    fprintf(stderr, "picture alloc failed\n");
    WebPAnimEncoderDelete(enc);
    return 1;
  }

  // First frame is a keyframe. Mixed mode encodes a lossless candidate, then
  // a lossy one; the hook aborts the second encode.
  if (WebPAnimEncoderAdd(enc, &frame, 0, &config)) {
    fprintf(stderr, "expected add to fail\n");
    WebPPictureFree(&frame);
    WebPAnimEncoderDelete(enc);
    return 1;
  }
  fprintf(stderr, "add failed as expected: %d %s\n", frame.error_code,
          WebPAnimEncoderGetError(enc));

  WebPDataInit(&webp);
  WebPPictureFree(&frame);
  WebPAnimEncoderDelete(enc);
  WebPDataClear(&webp);
  return 0;
}
