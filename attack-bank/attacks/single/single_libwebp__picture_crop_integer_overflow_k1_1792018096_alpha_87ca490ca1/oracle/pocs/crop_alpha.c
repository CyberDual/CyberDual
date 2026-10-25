#include <stdlib.h>

#include "webp/encode.h"

int main(void) {
  WebPPicture picture;
  uint8_t* planes[4];
  const int width = 2;
  const int height = 4;
  const int y_stride = 1024;
  const int uv_stride = 1;
  const int a_stride = 2;

  if (!WebPPictureInit(&picture)) return 1;
  picture.width = width;
  picture.height = height;
  picture.colorspace = WEBP_YUV420A;
  picture.y_stride = y_stride;
  picture.uv_stride = uv_stride;
  picture.a_stride = a_stride;

  planes[0] = (uint8_t*)malloc((size_t)y_stride * height);
  planes[1] = (uint8_t*)malloc((size_t)uv_stride * 2);
  planes[2] = (uint8_t*)malloc((size_t)uv_stride * 2);
  planes[3] = (uint8_t*)malloc((size_t)a_stride * height);
  if (planes[0] == NULL || planes[1] == NULL || planes[2] == NULL ||
      planes[3] == NULL) {
    return 2;
  }
  picture.y = planes[0];
  picture.u = planes[1];
  picture.v = planes[2];
  picture.a = planes[3];

  if (!WebPPictureCrop(&picture, 0, 2, width, 2)) return 3;

  free(planes[0]);
  free(planes[1]);
  free(planes[2]);
  free(planes[3]);
  WebPPictureFree(&picture);
  return 0;
}
