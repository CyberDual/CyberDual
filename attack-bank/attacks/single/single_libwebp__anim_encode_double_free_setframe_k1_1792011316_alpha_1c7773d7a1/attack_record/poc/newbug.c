#include <string.h>

#include "webp/encode.h"
#include "webp/mux.h"

typedef struct {
  int encodes;
  int last_percent;
} ProgressState;

static int AbortOnThirdEncode(int percent, const WebPPicture* picture) {
  ProgressState* const state = (ProgressState*)picture->user_data;
  if (percent == 1 && (state->encodes == 0 || state->last_percent > 1)) {
    ++state->encodes;
    if (state->encodes == 3) return 0;
  }
  state->last_percent = percent;
  return 1;
}

static int MakeFrame(WebPPicture* const pic, int width, int height,
                     uint32_t color, ProgressState* const state) {
  int y, x;
  if (!WebPPictureInit(pic)) return 0;
  pic->use_argb = 1;
  pic->width = width;
  pic->height = height;
  if (!WebPPictureAlloc(pic)) return 0;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      pic->argb[y * pic->argb_stride + x] = color;
    }
  }
  pic->progress_hook = AbortOnThirdEncode;
  pic->user_data = state;
  return 1;
}

int main(void) {
  const int width = 8;
  const int height = 8;
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* enc;
  WebPConfig config;
  WebPPicture frame0, frame1;
  ProgressState state;

  memset(&state, 0, sizeof(state));
  memset(&frame0, 0, sizeof(frame0));
  memset(&frame1, 0, sizeof(frame1));

  if (!WebPAnimEncoderOptionsInit(&options) || !WebPConfigInit(&config)) {
    return 1;
  }
  options.kmin = 0;
  options.kmax = 1;
  options.minimize_size = 0;
  options.allow_mixed = 0;

  config.lossless = 1;
  config.method = 0;

  enc = WebPAnimEncoderNew(width, height, &options);
  if (enc == NULL) return 1;
  if (!MakeFrame(&frame0, width, height, 0xff204060u, &state) ||
      !MakeFrame(&frame1, width, height, 0xff80c040u, &state)) {
    WebPPictureFree(&frame0);
    WebPPictureFree(&frame1);
    WebPAnimEncoderDelete(enc);
    return 1;
  }

  if (WebPAnimEncoderAdd(enc, &frame0, 0, &config)) {
    WebPAnimEncoderAdd(enc, &frame1, 100, &config);
  }

  WebPPictureFree(&frame0);
  WebPPictureFree(&frame1);
  WebPAnimEncoderDelete(enc);
  return 0;
}
