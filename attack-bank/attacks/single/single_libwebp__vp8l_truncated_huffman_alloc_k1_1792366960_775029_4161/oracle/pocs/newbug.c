// Decodes a crafted lossless WebP image through the public API.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "webp/decode.h"

// A small (8x1) lossless WebP stream using meta Huffman codes: the 2x1
// Huffman image assigns pixel groups 0 and 2, so the decoder remaps
// 3 group slots onto 2 Huffman groups.
static const uint8_t kImage[] = {
    0x52, 0x49, 0x46, 0x46, 0x22, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x4c, 0x16, 0x00, 0x00, 0x00, 0x2f, 0x07, 0x00, 0x00,
    0x00, 0x84, 0x03, 0x08, 0x44, 0xf4, 0x3f, 0x8c, 0x88, 0xfe, 0x47, 0x44,
    0xf4, 0x3f, 0x22, 0xa2, 0xff, 0x01,
};

int main(void) {
  int width = 0, height = 0;
  uint8_t* const rgba = WebPDecodeRGBA(kImage, sizeof(kImage), &width, &height);
  if (rgba == NULL) {
    fprintf(stderr, "decode failed\n");
    return 1;
  }
  printf("decoded %dx%d\n", width, height);
  WebPFree(rgba);
  return 0;
}
