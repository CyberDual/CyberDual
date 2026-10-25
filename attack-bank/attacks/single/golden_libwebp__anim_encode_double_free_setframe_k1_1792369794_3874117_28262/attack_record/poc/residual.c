// Exercises WebPAnimEncoderAdd() (libwebpmux) with a user progress hook that
// aborts an encode in progress: the first (lossless) candidate of the frame
// completes, the hook aborts the second (lossy) candidate.
#include <stdio.h>

#include "webp/encode.h"
#include "webp/mux.h"

static int g_pass = 0;
static int g_last_percent = -1;

static int AbortOnSecondPass(int percent, const WebPPicture* pic) {
  (void)pic;
  if (percent <= g_last_percent) ++g_pass;  // A new encode pass started.
  g_last_percent = percent;
  return (g_pass == 0);
}

int main(void) {
  const int kWidth = 96, kHeight = 64;
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* enc;
  WebPPicture pic;
  WebPConfig config;
  int i;

  if (!WebPPictureInit(&pic)) return 1;
  pic.width = kWidth;
  pic.height = kHeight;
  pic.use_argb = 1;
  pic.progress_hook = AbortOnSecondPass;
  if (!WebPPictureAlloc(&pic)) return 1;
  for (i = 0; i < kWidth * kHeight; ++i) {
    pic.argb[i] = 0xff000000u | (uint32_t)(i * 2654435761u);
  }

  if (!WebPAnimEncoderOptionsInit(&options)) return 1;
  options.minimize_size = 1;  // Try the lossless and lossy candidates.
  options.allow_mixed = 1;
  enc = WebPAnimEncoderNew(kWidth, kHeight, &options);
  if (enc == NULL) return 1;

  if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, 75.f)) return 1;
  config.lossless = 0;

  // The first frame is laid out as a keyframe.
  if (!WebPAnimEncoderAdd(enc, &pic, 0, &config)) {
    fprintf(stderr, "WebPAnimEncoderAdd() failed: %s\n",
            WebPAnimEncoderGetError(enc));
  }

  WebPAnimEncoderDelete(enc);
  WebPPictureFree(&pic);
  fprintf(stderr, "completed without a crash\n");
  return 0;
}
