// Probe: import a raw RGB565 image through the public WebPImportRGB565()
// helper, encode the resulting picture with WebPEncode(), and decode the
// bitstream back with WebPDecodeRGBA(). Exits 0 on success.
#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "extras/extras.h"

int main(void) {
  const int width = 5;
  const int height = 3;
  const size_t num_pixels = (size_t)width * height;
  uint8_t* const rgb565 = (uint8_t*)malloc(2 * num_pixels);
  WebPConfig config;
  WebPPicture pic;
  WebPMemoryWriter wrt;
  uint8_t* rgba = NULL;
  int dec_w = 0, dec_h = 0;
  int ok = 0;
  size_t i;

  if (rgb565 == NULL) return 1;
  for (i = 0; i < num_pixels; ++i) {
    rgb565[2 * i + 0] = (uint8_t)(0x1fu | ((i * 7) << 5));
    rgb565[2 * i + 1] = (uint8_t)(0xe0u | ((i * 5) & 0x1f));
  }

  WebPConfigInit(&config);
  WebPPictureInit(&pic);
  WebPMemoryWriterInit(&wrt);
  pic.width = width;
  pic.height = height;
  pic.use_argb = 1;
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &wrt;

  if (!WebPImportRGB565(rgb565, &pic)) {
    fprintf(stderr, "WebPImportRGB565 failed\n");
    goto End;
  }
  if (!WebPEncode(&config, &pic)) {
    fprintf(stderr, "WebPEncode failed: %d\n", (int)pic.error_code);
    goto End;
  }
  rgba = WebPDecodeRGBA(wrt.mem, wrt.size, &dec_w, &dec_h);
  if (rgba == NULL || dec_w != width || dec_h != height) {
    fprintf(stderr, "WebPDecodeRGBA failed\n");
    goto End;
  }
  ok = 1;

 End:
  WebPFree(rgba);
  WebPPictureFree(&pic);
  WebPMemoryWriterClear(&wrt);
  free(rgb565);
  return ok ? 0 : 1;
}
