// Triggers a double-free in WebPAnimEncoderAdd() when a user progress_hook
// aborts a candidate encode after a previous candidate of the same frame was
// already picked as the best candidate.
//
// The animation encoder evaluates candidates in this order for a sub-frame
// with minimize_size=1 and allow_mixed=1:
//   #0 frame1 lossless (key frame), #1 frame1 lossy,
//   #2 frame2 dispose-none lossless, #3 frame2 dispose-none lossy,
//   #4 frame2 dispose-background lossless, #5 frame2 dispose-background lossy.
// After encode #3 completes, one of the dispose-none candidates has been
// picked as the best candidate and its bitstream buffer is aliased into
// encoded_frame->sub_frame.bitstream. Aborting encode #4 makes
// GenerateCandidates() fail; SetFrame()'s error cleanup then releases the
// best candidate's buffer, and CacheFrame() releases it a second time via
// FrameRelease().
#include <stdio.h>
#include <stdlib.h>
#include "webp/encode.h"
#include "webp/mux.h"

#define WIDTH 32
#define HEIGHT 32

// Number of candidate encodes that must fully complete before the progress
// hook aborts the next one (i.e. abort the encode with 0-based index 4).
#define ABORT_AFTER_COMPLETED 4

typedef struct {
  int completed;  // Number of finished WebPEncode() calls (100% reports).
} HookState;

static int ProgressHook(int percent, const WebPPicture* pic) {
  HookState* const st = (HookState*)pic->user_data;
  if (st->completed == ABORT_AFTER_COMPLETED) {
    return 0;  // User abort: cancels the encode in progress.
  }
  if (percent == 100) ++st->completed;
  return 1;
}

static void FillFrame(WebPPicture* const pic, int frame_index) {
  int x, y;
  for (y = 0; y < HEIGHT; ++y) {
    uint32_t* const row = pic->argb + y * pic->argb_stride;
    for (x = 0; x < WIDTH; ++x) {
      row[x] = 0xff000000u | ((uint32_t)(x * 8) << 16) |
               ((uint32_t)(y * 8) << 8) | 0x40u;
    }
  }
  if (frame_index > 0) {
    // Paint a clearly different block so both the lossless and lossy change
    // rectangles are non-empty.
    for (y = 8; y < 24; ++y) {
      uint32_t* const row = pic->argb + y * pic->argb_stride;
      for (x = 8; x < 24; ++x) {
        row[x] = ((x + y) & 1) ? 0xffffffffu : 0xff000000u;
      }
    }
  }
}

int main(void) {
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* enc;
  WebPConfig config;
  WebPPicture pic;
  WebPData webp_data;
  HookState st;
  int ok;

  st.completed = 0;

  if (!WebPAnimEncoderOptionsInit(&options)) return 2;
  options.minimize_size = 1;
  options.allow_mixed = 1;

  enc = WebPAnimEncoderNew(WIDTH, HEIGHT, &options);
  if (enc == NULL) return 2;

  if (!WebPConfigInit(&config)) return 2;
  config.lossless = 1;

  if (!WebPPictureInit(&pic)) return 2;
  pic.width = WIDTH;
  pic.height = HEIGHT;
  pic.use_argb = 1;
  if (!WebPPictureAlloc(&pic)) return 2;
  pic.progress_hook = ProgressHook;
  pic.user_data = &st;

  FillFrame(&pic, 0);
  if (!WebPAnimEncoderAdd(enc, &pic, 0, &config)) {
    fprintf(stderr, "add(frame 1) failed: %s\n", WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    WebPPictureFree(&pic);
    return 3;
  }

  FillFrame(&pic, 1);
  ok = WebPAnimEncoderAdd(enc, &pic, 100, &config);
  if (ok) {
    WebPDataInit(&webp_data);
    if (WebPAnimEncoderAdd(enc, NULL, 200, &config) &&
        WebPAnimEncoderAssemble(enc, &webp_data)) {
      WebPDataClear(&webp_data);
    }
    fprintf(stderr, "unexpected: frame 2 was encoded without an abort\n");
    WebPAnimEncoderDelete(enc);
    WebPPictureFree(&pic);
    return 4;
  }

  // The user abort surfaced as an encoding error; on a fixed tree this is
  // handled gracefully.
  fprintf(stderr, "add(frame 2) failed as expected: %s\n",
          WebPAnimEncoderGetError(enc));
  WebPAnimEncoderDelete(enc);
  WebPPictureFree(&pic);
  printf("clean exit\n");
  return 0;
}
