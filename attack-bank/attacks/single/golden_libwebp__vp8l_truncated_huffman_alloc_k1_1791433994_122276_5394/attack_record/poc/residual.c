// PoC: decode a crafted lossless (VP8L) WebP through the public decode API.
//
// The crafted image uses a meta-Huffman (entropy) image whose single pixel
// selects Huffman group index 1 while the entropy image has only one pixel, so
// num_htree_groups_max (2) exceeds huffman_pixs (1) and the decoder takes the
// sparse group-remapping path in ReadHuffmanCodes().
//
// cmd: cc -fsanitize=address pocs/residual.c -I. -Lbuild -lwebp -lwebpdecoder ... ; ./a.out
#include <stdio.h>
#include <stdlib.h>
#include "src/webp/decode.h"

static const unsigned char kWebP[] = {
0x52,0x49,0x46,0x46,0x1e,0x00,0x00,0x00,0x57,0x45,0x42,0x50,0x56,0x50,0x38,0x4c,
0x12,0x00,0x00,0x00,0x2f,0x00,0x00,0x00,0x00,0x84,0x8c,0x88,0x88,0x88,0x88,0x88,
0x88,0x00,0x00,0x00,0x00,0x00,
};
static const unsigned int kWebPLen = 38;

int main(void) {
  int w = 0, h = 0;
  uint8_t* out = WebPDecodeRGBA(kWebP, kWebPLen, &w, &h);
  if (out != NULL) {
    WebPFree(out);
    printf("decoded %dx%d (no fault)\n", w, h);
  } else {
    printf("decode returned NULL (no fault)\n");
  }
  return 0;
}
