// Reproducer: out-of-bounds read in the packed 16-bit importers.
//
// Drives WebPImportRGB565() / WebPImportRGB4444() (public extras API,
// extras/extras.h) with an in-contract input: a heap buffer holding
// exactly width * height * 2 packed samples and a WebPPicture whose
// width/height are set beforehand, as documented.
//
// On a tree whose import loop iterates one pixel too far, the last row
// reads two bytes past the end of that buffer (ASan abort). On a tree
// with the correct loop bound the import completes and every expanded
// pixel matches the expected ARGB values below.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extras.h"
#include "webp/encode.h"

#define W 33
#define H 17

static int CheckPixels(const WebPPicture* const pic, uint32_t expected,
                       const char* label) {
  int x, y;
  for (y = 0; y < pic->height; ++y) {
    for (x = 0; x < pic->width; ++x) {
      const uint32_t argb = pic->argb[y * pic->argb_stride + x];
      if (argb != expected) {
        fprintf(stderr, "%s: pixel (%d,%d) = 0x%08x, expected 0x%08x\n", label,
                x, y, argb, expected);
        return 0;
      }
    }
  }
  return 1;
}

int main(void) {
  const size_t size = (size_t)W * H * 2;
  uint8_t* buf = (uint8_t*)malloc(size);  // exactly width*height*2 bytes
  WebPPicture pic;
  int ok = 1;

  if (buf == NULL) return 1;
  memset(buf, 0xa5, size);

  // 0xa5,0xa5 through the fixed 565 expansion: r=0xa0|0x05, g=0xb4|0x02,
  // b=0x28|0x01, a=0xff.
  if (!WebPPictureInit(&pic)) return 1;
  pic.width = W;
  pic.height = H;
  if (!WebPImportRGB565(buf, &pic)) return 1;
  printf("WebPImportRGB565 ok\n");
  ok &= CheckPixels(&pic, 0xffa5b629u, "rgb565");
  WebPPictureFree(&pic);

  // 0xa5,0xa5 through the fixed 4444 expansion: r=0xa0|0x0a, g=0x50|0x05,
  // b=0xa0|0x0a, a=0x50|0x05.
  if (!WebPPictureInit(&pic)) return 1;
  pic.width = W;
  pic.height = H;
  if (!WebPImportRGB4444(buf, &pic)) return 1;
  printf("WebPImportRGB4444 ok\n");
  ok &= CheckPixels(&pic, 0x55aa55aau, "rgb4444");
  WebPPictureFree(&pic);

  free(buf);
  if (!ok) {
    fprintf(stderr, "pixel mismatch\n");
    return 1;
  }
  printf("all pixels correct\n");
  return 0;
}
