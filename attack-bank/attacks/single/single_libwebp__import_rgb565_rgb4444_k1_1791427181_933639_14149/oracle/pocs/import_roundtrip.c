#include <stdint.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const uint8_t rgba[] = {
      0x20, 0x40, 0x60, 0xff,
      0x80, 0xa0, 0xc0, 0xff,
      0xe0, 0x70, 0x30, 0xff,
  };
  uint8_t *encoded = NULL, *decoded = NULL, *rgb565 = NULL;
  size_t encoded_size;
  WebPPicture pic;
  int width, height, x, ok;

  encoded_size = WebPEncodeLosslessRGBA(rgba, 3, 1, 12, &encoded);
  if (encoded_size == 0) return 1;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != 3 || height != 1) {
    WebPFree(decoded);
    return 1;
  }
  rgb565 = (uint8_t*)malloc((size_t)width * 2);
  if (rgb565 == NULL) {
    WebPFree(decoded);
    return 1;
  }
  for (x = 0; x < width; ++x) {
    const uint8_t* const p = decoded + 4 * x;
    rgb565[2 * x] = (p[0] & 0xf8) | (p[1] >> 5);
    rgb565[2 * x + 1] = ((p[1] & 0x1c) << 3) | (p[2] >> 3);
  }
  WebPFree(decoded);

  if (!WebPPictureInit(&pic)) {
    free(rgb565);
    return 1;
  }
  pic.width = width;
  pic.height = height;
  ok = WebPImportRGB565(rgb565, &pic);
  WebPPictureFree(&pic);
  free(rgb565);
  return ok ? 0 : 1;
}
