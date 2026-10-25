// Drives libwebp through its public API only:
//   1. encode a lossy WebP with a non-trivial alpha channel (WebPEncode),
//   2. decode the bytes back (WebPDecode) requesting alpha dithering
//      (WebPDecoderOptions::alpha_dithering_strength, valid range [0, 100]).
//
// Both steps use in-contract input produced/consumed through <webp/encode.h>
// and <webp/decode.h>.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define WIDTH 320
#define HEIGHT 240

static int DecodeWithAlphaDithering(const uint8_t* data, size_t size,
                                    const char* tag) {
  WebPDecoderConfig config;
  VP8StatusCode status;
  if (!WebPInitDecoderConfig(&config)) {
    fprintf(stderr, "%s: WebPInitDecoderConfig failed\n", tag);
    return 0;
  }
  if (WebPGetFeatures(data, size, &config.input) != VP8_STATUS_OK) {
    fprintf(stderr, "%s: WebPGetFeatures failed\n", tag);
    return 0;
  }
  if (!config.input.has_alpha) {
    fprintf(stderr, "%s: encoded image unexpectedly lost its alpha\n", tag);
    return 0;
  }
  config.options.alpha_dithering_strength = 100;
  config.output.colorspace = MODE_RGBA;
  status = WebPDecode(data, size, &config);
  if (status != VP8_STATUS_OK) {
    fprintf(stderr, "%s: WebPDecode failed with status %d\n", tag,
            (int)status);
    WebPFreeDecBuffer(&config.output);
    return 0;
  }
  WebPFreeDecBuffer(&config.output);
  return 1;
}

static int EncodeAndProbe(const WebPConfig* config, const uint8_t* rgba,
                          const char* tag) {
  WebPPicture picture;
  WebPMemoryWriter writer;
  int ok;
  if (!WebPPictureInit(&picture)) return 0;
  picture.width = WIDTH;
  picture.height = HEIGHT;
  picture.use_argb = 0;
  if (!WebPPictureImportRGBA(&picture, rgba, WIDTH * 4)) {
    WebPPictureFree(&picture);
    return 0;
  }
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(config, &picture)) {
    fprintf(stderr, "%s: WebPEncode failed (error %d)\n", tag,
            (int)picture.error_code);
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&picture);
    return 0;
  }
  WebPPictureFree(&picture);
  ok = DecodeWithAlphaDithering(writer.mem, writer.size, tag);
  WebPMemoryWriterClear(&writer);
  return ok;
}

int main(void) {
  static uint8_t rgba[WIDTH * HEIGHT * 4];
  WebPConfig config;
  int x, y;
  int ok = 1;

  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      uint8_t* const px = rgba + 4 * (y * WIDTH + x);
      px[0] = (uint8_t)(x * 3 + y);
      px[1] = (uint8_t)(x + y * 5);
      px[2] = (uint8_t)((x * x) / 7 + y * 2);
      px[3] = (uint8_t)(16 + ((x * 7 + y * 13) % 224));  // varying alpha
    }
  }

  // Variant 1: default settings (lossless-compressed alpha).
  if (!WebPConfigInit(&config)) return 2;
  config.quality = 75.f;
  ok &= EncodeAndProbe(&config, rgba, "lossless-alpha");

  // Variant 2: uncompressed (raw) alpha plane.
  if (!WebPConfigInit(&config)) return 2;
  config.quality = 75.f;
  config.alpha_compression = 0;
  ok &= EncodeAndProbe(&config, rgba, "raw-alpha");

  if (!ok) return 1;
  printf("ok\n");
  return 0;
}
