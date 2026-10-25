// Crop a single-column strip whose lower half falls outside the picture.
// Drives WebPPictureCrop() through the public encode API.
#include <stdio.h>

#include "webp/encode.h"

#define W 128
#define H 128

int main(void) {
  uint8_t rgba[W * H * 4];
  WebPPicture pic;
  int x, y;

  for (y = 0; y < H; ++y) {
    for (x = 0; x < W; ++x) {
      uint8_t* const px = rgba + 4 * (y * W + x);
      px[0] = (uint8_t)x;
      px[1] = (uint8_t)y;
      px[2] = 0x80;
      px[3] = 0xff;
    }
  }

  if (!WebPPictureInit(&pic)) return 1;
  pic.use_argb = 1;
  pic.width = W;
  pic.height = H;
  if (!WebPPictureImportRGBA(&pic, rgba, 4 * W)) return 1;

  // Keep column 0, rows 64..191 of a 128-row picture.
  if (!WebPPictureCrop(&pic, 0, 64, 1, 128)) {
    fprintf(stderr, "crop rejected\n");
    WebPPictureFree(&pic);
    return 0;
  }
  fprintf(stderr, "cropped to %dx%d\n", pic.width, pic.height);
  WebPPictureFree(&pic);
  return 0;
}
