#include <stddef.h>

#include "webp/decode.h"

static void PutBits(uint8_t* const data, size_t* const bit_pos,
                    uint32_t value, unsigned int num_bits) {
  unsigned int i;
  for (i = 0; i < num_bits; ++i) {
    if (value & 1u) {
      data[*bit_pos >> 3] |= (uint8_t)(1u << (*bit_pos & 7));
    }
    ++*bit_pos;
    value >>= 1;
  }
}

static void PutLE32(uint8_t* const data, uint32_t value) {
  data[0] = (uint8_t)(value >> 0);
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

int main(void) {
  uint8_t vp8l[16] = {0};
  uint8_t webp[12 + 8 + sizeof(vp8l)];
  size_t bit_pos = 0;
  int width = 0;
  int height = 0;
  uint8_t* decoded;

  PutBits(vp8l, &bit_pos, 0x2f, 8);
  PutBits(vp8l, &bit_pos, 0, 14);
  PutBits(vp8l, &bit_pos, 0, 14);
  PutBits(vp8l, &bit_pos, 0, 1);
  PutBits(vp8l, &bit_pos, 0, 3);
  PutBits(vp8l, &bit_pos, 0, 1);
  PutBits(vp8l, &bit_pos, 1, 1);
  PutBits(vp8l, &bit_pos, 12, 4);
  PutBits(vp8l, &bit_pos, 0, 1);

  webp[0] = 'R';
  webp[1] = 'I';
  webp[2] = 'F';
  webp[3] = 'F';
  PutLE32(webp + 4, (uint32_t)(4 + 8 + sizeof(vp8l)));
  webp[8] = 'W';
  webp[9] = 'E';
  webp[10] = 'B';
  webp[11] = 'P';
  webp[12] = 'V';
  webp[13] = 'P';
  webp[14] = '8';
  webp[15] = 'L';
  PutLE32(webp + 16, (uint32_t)sizeof(vp8l));
  for (size_t i = 0; i < sizeof(vp8l); ++i) {
    webp[20 + i] = vp8l[i];
  }

  decoded = WebPDecodeRGBA(webp, sizeof(webp), &width, &height);
  WebPFree(decoded);
  (void)width;
  (void)height;
  return 0;
}
