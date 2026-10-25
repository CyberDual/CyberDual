#include <stdlib.h>
#include <string.h>

#include "extras.h"
#include "webp/encode.h"

int main(void) {
  const int width = 7;
  const int height = 5;
  const size_t size = (size_t)width * height * 2;
  uint8_t* data = (uint8_t*)malloc(size);
  WebPPicture pic;

  if (data == NULL) return 1;
  memset(data, 0xa5, size);

  if (!WebPPictureInit(&pic)) {
    free(data);
    return 1;
  }
  pic.width = width;
  pic.height = height;

  if (!WebPImportRGB565(data, &pic)) {
    WebPPictureFree(&pic);
    free(data);
    return 1;
  }
  WebPPictureFree(&pic);
  free(data);
  return 0;
}
