#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"

int main(void) {
  WebPPicture src;
  WebPPicture view;
  int y;

  if (!WebPPictureInit(&src)) return 2;
  src.use_argb = 0;
  src.width = 32;
  src.height = 32;
  if (!WebPPictureAlloc(&src)) return 2;
  for (y = 0; y < src.height; ++y) {
    int x;
    for (x = 0; x < src.width; ++x) {
      src.y[y * src.y_stride + x] = (uint8_t)(y * 32 + x);
    }
  }
  for (y = 0; y < (src.height + 1) / 2; ++y) {
    int x;
    for (x = 0; x < (src.width + 1) / 2; ++x) {
      src.u[y * src.uv_stride + x] = 128;
      src.v[y * src.uv_stride + x] = 128;
    }
  }

  if (!WebPPictureInit(&view)) return 2;
  if (!WebPPictureView(&src, 0, 0, 16, 16, &view)) return 2;
  if (!WebPPictureCrop(&view, 0, 0, view.width, view.height)) return 2;

  WebPPictureFree(&view);
  WebPPictureFree(&src);
  printf("ok %d\n", view.width);
  return 0;
}
