// Residual-defect PoC for WebPImportRGB565().
//
// Calls the public libwebp "extras" helper WebPImportRGB565() with a
// tightly-sized, valid RGB565 source buffer for an odd-width picture.
// On the shipped tree this reads 2 bytes past the end of the source
// buffer (heap-buffer-overflow) while converting the trailing column.
#include <stdlib.h>
#include <string.h>

#include "extras/extras.h"
#include "webp/encode.h"

int main(void) {
  const int width = 3;   // odd width: in-contract, ordinary input
  const int height = 1;
  WebPPicture pic;
  uint8_t* rgb565;

  if (!WebPPictureInit(&pic)) {
    return 1;
  }
  pic.width = width;
  pic.height = height;

  // Exactly as large as the documented contract requires: 2 bytes per
  // pixel, tightly packed, no padding.
  rgb565 = (uint8_t*)malloc((size_t)2 * width * height);
  if (rgb565 == NULL) return 1;
  memset(rgb565, 0x55, (size_t)2 * width * height);

  WebPImportRGB565(rgb565, &pic);

  WebPPictureFree(&pic);
  free(rgb565);
  return 0;
}
