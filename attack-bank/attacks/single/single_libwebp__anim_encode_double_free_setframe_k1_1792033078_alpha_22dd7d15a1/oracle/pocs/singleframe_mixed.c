// PoC: single-frame animation encoded in mixed mode.
//
// WebPAnimEncoderAssemble() runs OptimizeSingleFrame() on the single flushed
// frame, which re-encodes the decoded canvas with both the frame's config and
// its reversed (lossless<->lossy) counterpart and keeps the smaller output.
//
// Build: see the command in submission.json.
#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"
#include "webp/mux.h"

#define W 64
#define H 64

static void FillPicture(WebPPicture* const pic) {
  int x, y;
  for (y = 0; y < H; ++y) {
    uint32_t* const row = pic->argb + y * pic->argb_stride;
    for (x = 0; x < W; ++x) {
      if (x >= W / 4 && x < 3 * W / 4 && y >= H / 4 && y < 3 * H / 4) {
        // 4x4 blocks of flat color inside the frame rectangle: compresses
        // much smaller in lossless than a lossy re-encode of the canvas.
        const uint8_t r = (uint8_t)((x / 4) * 16);
        const uint8_t g = (uint8_t)((y / 4) * 16);
        const uint8_t b = (uint8_t)(((x / 4) + (y / 4)) * 8);
        row[x] = 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      } else {
        row[x] = 0;  // Transparent border.
      }
    }
  }
}

int main(void) {
  WebPAnimEncoderOptions opts;
  WebPAnimEncoder* enc;
  WebPPicture pic;
  WebPConfig config;
  WebPData webp_data;

  if (!WebPAnimEncoderOptionsInit(&opts)) return 1;
  opts.allow_mixed = 1;
  opts.verbose = 0;

  enc = WebPAnimEncoderNew(W, H, &opts);
  if (enc == NULL) return 1;

  if (!WebPPictureInit(&pic)) return 1;
  pic.width = W;
  pic.height = H;
  pic.use_argb = 1;
  if (!WebPPictureAlloc(&pic)) return 1;
  FillPicture(&pic);

  if (!WebPConfigInit(&config)) return 1;
  config.lossless = 0;
  config.quality = 80.0f;

  if (!WebPAnimEncoderAdd(enc, &pic, 0, &config)) {
    fprintf(stderr, "WebPAnimEncoderAdd failed: %s\n",
            WebPAnimEncoderGetError(enc));
    return 1;
  }
  if (!WebPAnimEncoderAdd(enc, NULL, 100, NULL)) {
    fprintf(stderr, "final WebPAnimEncoderAdd failed: %s\n",
            WebPAnimEncoderGetError(enc));
    return 1;
  }

  WebPDataInit(&webp_data);
  if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
    fprintf(stderr, "WebPAnimEncoderAssemble failed: %s\n",
            WebPAnimEncoderGetError(enc));
    return 1;
  }
  fprintf(stderr, "assembled %zu bytes\n", webp_data.size);

  WebPDataClear(&webp_data);
  WebPAnimEncoderDelete(enc);
  WebPPictureFree(&pic);
  return 0;
}
