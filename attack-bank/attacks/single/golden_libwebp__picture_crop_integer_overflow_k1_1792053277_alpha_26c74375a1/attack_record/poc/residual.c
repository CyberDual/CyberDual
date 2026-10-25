#include <stdint.h>
#include <stdio.h>

#include "webp/encode.h"

/* Crop a 16x16 picture with an origin past the right edge. The rectangle is
   outside the source, so WebPPictureCrop is required to fail. */
int main(void) {
  WebPPicture pic;

  if (!WebPPictureInit(&pic)) return 2;
  pic.use_argb = 1;
  pic.width = 16;
  pic.height = 16;
  if (!WebPPictureAlloc(&pic)) return 3;

  if (!WebPPictureCrop(&pic, 400, 0, 1, 1)) {
    WebPPictureFree(&pic);
    return 0;
  }

  if (pic.argb != NULL) {
    volatile uint32_t pixel = pic.argb[0];
    (void)pixel;
  }
  printf("crop accepted %dx%d\n", pic.width, pic.height);
  WebPPictureFree(&pic);
  return 1;
}
