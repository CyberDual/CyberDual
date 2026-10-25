// Probe: encode a lossy WebP with a (non-quantized) alpha channel, then
// decode it with alpha dithering requested through the public decoder
// options. Exits 0 on success; aborts under AddressSanitizer if the
// alpha plane handling is faulty.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define WIDTH 64
#define HEIGHT 512

// Make ASan abort() after reporting so the crash is unmistakable.
#if defined(__SANITIZE_ADDRESS__)
const char* __asan_default_options(void) { return "abort_on_error=1"; }
#endif

int main(void) {
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  WebPDecoderConfig dconfig;
  int x, y;
  int ok = 0;

  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) return 2;
  config.lossless = 0;
  config.quality = 75.f;
  config.alpha_compression = 0;  // raw alpha
  config.alpha_quality = 100;    // no levels quantization of alpha
  if (!WebPValidateConfig(&config)) return 2;

  picture.width = WIDTH;
  picture.height = HEIGHT;
  picture.use_argb = 1;
  if (!WebPPictureAlloc(&picture)) return 2;
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      uint32_t* const px = picture.argb + y * picture.argb_stride + x;
      const uint32_t a = (uint32_t)((x * 3 + y) & 0xff);  // non-opaque alpha
      *px = (a << 24) | (uint32_t)(x & 0xff) << 16 |
            (uint32_t)(y & 0xff) << 8 | (uint32_t)((x + y) & 0xff);
    }
  }
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "encode failed\n");
    goto End;
  }

  if (!WebPInitDecoderConfig(&dconfig)) goto End;
  dconfig.output.colorspace = MODE_RGBA;
  dconfig.options.alpha_dithering_strength = 100;
  if (WebPDecode(writer.mem, writer.size, &dconfig) != VP8_STATUS_OK) {
    fprintf(stderr, "decode failed\n");
    goto End;
  }
  WebPFreeDecBuffer(&dconfig.output);
  ok = 1;

End:
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&picture);
  if (ok) printf("ok\n");
  return ok ? 0 : 1;
}
