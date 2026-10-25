#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "webp/encode.h"

int main(void) {
  const int width = 4096;
  const int height = 2048;
  uint8_t* src;
  WebPPicture pic;
  int ok;

  src = (uint8_t*)calloc((size_t)width * (size_t)height, 2);
  if (src == NULL) return 2;
  if (!WebPPictureInit(&pic)) return 2;
  pic.width = width;
  pic.height = height;
  ok = WebPImportRGB565(src, &pic);
  printf("import=%d argb0=%08x\n", ok, pic.argb != NULL ? pic.argb[0] : 0);
  WebPPictureFree(&pic);
  free(src);
  return ok ? 0 : 1;
}
