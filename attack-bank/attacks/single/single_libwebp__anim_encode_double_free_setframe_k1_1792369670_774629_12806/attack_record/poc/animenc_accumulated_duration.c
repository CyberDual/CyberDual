// Animated WebP encoder probe: builds a small animation whose frames carry
// large (but individually valid) timestamp gaps. Identical consecutive frames
// are merged by the encoder, so the accumulated duration of a merged frame
// eventually exceeds MAX_DURATION and the encoder has to split the merged
// duration across an inserted frame. The resulting animation is assembled and
// released through the public mux API.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/mux.h"

#define CANVAS_W 16
#define CANVAS_H 16
#define FRAME_GAP_MS 10000000  // < MAX_DURATION (1 << 24)

// Produces a small WebP bitstream and decodes it back to RGBA, modeling an
// image ingest pipeline.
static uint8_t* LoadFrameRGBA(void) {
  uint8_t rgba[CANVAS_W * CANVAS_H * 4];
  for (int i = 0; i < CANVAS_W * CANVAS_H; ++i) {
    rgba[i * 4 + 0] = 17;
    rgba[i * 4 + 1] = 91;
    rgba[i * 4 + 2] = 133;
    rgba[i * 4 + 3] = 0xff;
  }
  uint8_t* webp = NULL;
  const size_t webp_size =
      WebPEncodeRGBA(rgba, CANVAS_W, CANVAS_H, CANVAS_W * 4, 90.f, &webp);
  if (webp_size == 0 || webp == NULL) return NULL;

  int width = 0, height = 0;
  uint8_t* const decoded = WebPDecodeRGBA(webp, webp_size, &width, &height);
  WebPFree(webp);
  if (decoded == NULL || width != CANVAS_W || height != CANVAS_H) {
    WebPFree(decoded);
    return NULL;
  }
  return decoded;
}

int main(void) {
  WebPAnimEncoderOptions options;
  if (!WebPAnimEncoderOptionsInit(&options)) return 1;

  WebPAnimEncoder* const enc =
      WebPAnimEncoderNew(CANVAS_W, CANVAS_H, &options);
  if (enc == NULL) return 1;

  WebPConfig config;
  if (!WebPConfigInit(&config)) return 1;
  config.lossless = 1;

  // Three identical frames spaced by FRAME_GAP_MS: the second and third merge
  // into the first frame, accumulating its duration past MAX_DURATION.
  for (int frame = 0; frame < 3; ++frame) {
    uint8_t* const rgba = LoadFrameRGBA();
    if (rgba == NULL) return 1;

    WebPPicture pic;
    if (!WebPPictureInit(&pic)) return 1;
    pic.width = CANVAS_W;
    pic.height = CANVAS_H;
    pic.use_argb = 1;
    if (!WebPPictureAlloc(&pic)) return 1;
    if (!WebPPictureImportRGBA(&pic, rgba, CANVAS_W * 4)) return 1;
    WebPFree(rgba);

    const int timestamp_ms = frame * FRAME_GAP_MS;
    if (!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config)) {
      fprintf(stderr, "WebPAnimEncoderAdd failed: %s\n",
              WebPAnimEncoderGetError(enc));
      WebPPictureFree(&pic);
      WebPAnimEncoderDelete(enc);
      return 1;
    }
    WebPPictureFree(&pic);
  }

  if (!WebPAnimEncoderAdd(enc, NULL, 3 * FRAME_GAP_MS, NULL)) {
    fprintf(stderr, "Final WebPAnimEncoderAdd failed: %s\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    return 1;
  }

  WebPData webp_data;
  WebPDataInit(&webp_data);
  if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
    fprintf(stderr, "WebPAnimEncoderAssemble failed: %s\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    return 1;
  }
  printf("assembled %u bytes\n", (unsigned)webp_data.size);

  WebPDataClear(&webp_data);
  WebPAnimEncoderDelete(enc);
  printf("done\n");
  return 0;
}
