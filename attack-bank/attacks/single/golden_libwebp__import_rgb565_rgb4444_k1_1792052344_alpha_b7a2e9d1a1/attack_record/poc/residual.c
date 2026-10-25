#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extras/extras.h"
#include "webp/encode.h"

/* In-contract packed RGB565: width*height samples, 2 bytes each, no padding. */
int main(void) {
  enum { kWidth = 5, kHeight = 3 };
  uint8_t rgb565[kWidth * kHeight * 2];
  WebPPicture pic;
  int i;
  volatile uint32_t sink = 0;

  for (i = 0; i < kWidth * kHeight * 2; ++i) {
    rgb565[i] = (uint8_t)(0x20 + i * 13);
  }
  if (!WebPPictureInit(&pic)) return 1;
  pic.width = kWidth;
  pic.height = kHeight;
  if (!WebPImportRGB565(rgb565, &pic)) return 2;
  for (i = 0; i < kWidth * kHeight; ++i) sink += pic.argb[i];
  WebPPictureFree(&pic);
  return sink == 0xffffffffu ? 3 : 0;
}
