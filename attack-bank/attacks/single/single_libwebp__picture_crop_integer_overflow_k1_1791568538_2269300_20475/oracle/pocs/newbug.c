// Drives WebPPictureCrop (public encoder API) with an in-contract crop
// rectangle whose bottom edge sits exactly at the picture height. On the
// patched tree the rectangle is accepted and the crop copies one row past the
// end of the source ARGB buffer; on the parent tree the rectangle is rejected
// and nothing is copied.
#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"

int main(void) {
  const int kWidth = 256;
  const int kHeight = 8;
  WebPPicture pic;

  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "WebPPictureInit failed\n");
    return 2;
  }
  pic.use_argb = 1;
  pic.width = kWidth;
  pic.height = kHeight;
  if (!WebPPictureAlloc(&pic)) {
    fprintf(stderr, "WebPPictureAlloc failed\n");
    return 2;
  }

  // A plain opaque image; ordinary valid pixel content.
  for (int y = 0; y < kHeight; ++y) {
    uint32_t* const row = pic.argb + (size_t)y * pic.argb_stride;
    for (int x = 0; x < kWidth; ++x) row[x] = 0xff202020u;
  }

  // Full-width crop whose height reaches one row beyond the picture.
  const int ok = WebPPictureCrop(&pic, 0, 0, kWidth, kHeight + 1);
  fprintf(stderr, "WebPPictureCrop returned %d\n", ok);

  WebPPictureFree(&pic);
  return ok ? 0 : 1;
}
