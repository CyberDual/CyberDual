// Probe: lossy WebP with a lossless-compressed alpha plane, decoded with
// alpha_dithering_strength > 0 (a documented public decoding option).
//
// Builds the input bytes with the public encoder (WebPEncodeRGBA) and decodes
// them back with the public decoder (WebPDecode) in one program.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int kWidth = 320;
  const int kHeight = 320;
  unsigned char* rgba;
  uint8_t* encoded = NULL;
  size_t encoded_size;
  int x, y, ok = 1;

  // An image whose alpha channel is a fine gradient: the encoder keeps the
  // alpha plane losslessly compressed, with several hundred distinct levels.
  rgba = (unsigned char*)malloc((size_t)kWidth * kHeight * 4);
  if (rgba == NULL) return 1;
  for (y = 0; y < kHeight; ++y) {
    for (x = 0; x < kWidth; ++x) {
      unsigned char* const px =
          rgba + 4 * ((size_t)y * kWidth + x);
      px[0] = (unsigned char)(x * 3 + y);
      px[1] = (unsigned char)(y * 7);
      px[2] = (unsigned char)(x ^ y);
      px[3] = (unsigned char)(x + 1);  // 1..320: many distinct alpha levels
    }
  }

  encoded_size =
      WebPEncodeRGBA(rgba, kWidth, kHeight, kWidth * 4, 90.f, &encoded);
  free(rgba);
  if (encoded == NULL || encoded_size == 0) return 1;

  {
    // Decode requesting alpha dithering. This is an ordinary, documented use
    // of the public API (WebPDecoderOptions.alpha_dithering_strength).
    WebPDecoderConfig config;
    WebPDecBuffer* out;
    if (!WebPInitDecoderConfig(&config)) {
      WebPFree(encoded);
      return 1;
    }
    if (WebPGetFeatures(encoded, encoded_size, &config.input) != VP8_STATUS_OK ||
        !config.input.has_alpha) {
      // The encoder did not keep an alpha plane; the probe is not meaningful.
      WebPFree(encoded);
      return 1;
    }
    config.output.colorspace = MODE_RGBA;
    config.options.alpha_dithering_strength = 100;
    if (WebPDecode(encoded, encoded_size, &config) != VP8_STATUS_OK) {
      ok = 0;
    }
    out = &config.output;
    printf("decoded %s: %dx%d\n", ok ? "ok" : "failed", out->width,
           out->height);
    WebPFreeDecBuffer(out);
  }

  WebPFree(encoded);
  return ok ? 0 : 1;
}
