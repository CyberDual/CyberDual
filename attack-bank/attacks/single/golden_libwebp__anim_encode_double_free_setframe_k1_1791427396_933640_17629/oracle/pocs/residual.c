#include <stdio.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/mux.h"

enum { WIDTH = 48, HEIGHT = 48 };

typedef struct {
  int last_percent;
  int stopped;
} Progress;

static int StopSecondCandidate(int percent, const WebPPicture* picture) {
  Progress* const progress = (Progress*)picture->user_data;
  if (percent < progress->last_percent) {
    progress->stopped = 1;
    return 0;
  }
  progress->last_percent = percent;
  return 1;
}

static int MakePicture(int seed, WebPPicture* picture) {
  uint8_t rgba[WIDTH * HEIGHT * 4];
  uint8_t* encoded;
  uint8_t* decoded;
  size_t size;
  int x, y, width, height;
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      const int i = 4 * (y * WIDTH + x);
      rgba[i + 0] = (uint8_t)(x * 7 + y * 3 + seed * 37);
      rgba[i + 1] = (uint8_t)(x * 2 + y * 11 + seed * 53);
      rgba[i + 2] = (uint8_t)(x * 13 + y * 5 + seed * 19);
      rgba[i + 3] = 255;
    }
  }
  size = WebPEncodeLosslessRGBA(rgba, WIDTH, HEIGHT, WIDTH * 4, &encoded);
  if (size == 0) return 0;
  decoded = WebPDecodeRGBA(encoded, size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != WIDTH || height != HEIGHT) {
    WebPFree(decoded);
    return 0;
  }
  if (!WebPPictureInit(picture)) {
    WebPFree(decoded);
    return 0;
  }
  picture->width = WIDTH;
  picture->height = HEIGHT;
  if (!WebPPictureImportRGBA(picture, decoded, WIDTH * 4)) {
    WebPFree(decoded);
    return 0;
  }
  WebPFree(decoded);
  return 1;
}

int main(int argc, char** argv) {
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* encoder;
  WebPConfig config;
  WebPPicture first, second;
  WebPData output;
  Progress progress = {0, 0};
  const int normal = argc > 1 && argv[1][0] == 'n';
  int added;
  if (!WebPAnimEncoderOptionsInit(&options) || !WebPConfigInit(&config)) {
    return 2;
  }
  options.allow_mixed = 1;
  options.minimize_size = 1;
  config.lossless = 1;
  if (!MakePicture(0, &first) || !MakePicture(1, &second)) return 2;
  encoder = WebPAnimEncoderNew(WIDTH, HEIGHT, &options);
  if (encoder == NULL) return 2;
  if (!WebPAnimEncoderAdd(encoder, &first, 0, &config)) return 2;
  if (!normal) {
    second.progress_hook = StopSecondCandidate;
    second.user_data = &progress;
  }
  added = WebPAnimEncoderAdd(encoder, &second, 100, &config);
  if (normal && added) {
    WebPDataInit(&output);
    added = WebPAnimEncoderAssemble(encoder, &output);
    WebPDataClear(&output);
  }
  WebPAnimEncoderDelete(encoder);
  WebPPictureFree(&first);
  WebPPictureFree(&second);
  if (normal) return added ? 0 : 2;
  if (added || !progress.stopped) {
    fprintf(stderr, "second candidate was not interrupted\n");
    return 2;
  }
  return 0;
}
