// Probe for the libwebp evaluation case.
//
// Crafts a valid lossless WebP whose meta-Huffman image uses group indices
// {0..200, 204}: 202 distinct values with a maximum index of 204, i.e.
// num_htree_groups == 202, num_htree_groups_max == 205 (and 205 > 200, so
// the mapping/compaction path runs).
//
// The image is decoded through the public API WebPDecodeRGBA().

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"

//------------------------------------------------------------------------------

#define MAIN_W 256
#define MAIN_H 16
#define HPREC 2                 // Huffman subsampling precision
#define HXSIZE (MAIN_W >> HPREC)  // 64
#define HYSIZE (MAIN_H >> HPREC)  // 4
#define HPIX (HXSIZE * HYSIZE)    // 256

#define GROUP_MAX 205  // num_htree_groups_max
#define GROUPS_USED 202
#define TOP_GROUP 204  // largest group index used by the image

typedef struct {
  uint8_t bytes[2048];
  int bit_pos;
} BitWriter;

static void PutBits(BitWriter* const bw, uint32_t value, int nbits) {
  int i;
  for (i = 0; i < nbits; ++i) {
    if (value & (1u << i)) {
      bw->bytes[bw->bit_pos >> 3] |= (uint8_t)(1u << (bw->bit_pos & 7));
    }
    ++bw->bit_pos;
  }
}

// Emits a canonical prefix code, most-significant bit first.
static void PutSymbol(BitWriter* const bw, uint32_t code, int nbits) {
  int i;
  for (i = nbits - 1; i >= 0; --i) PutBits(bw, (code >> i) & 1u, 1);
}

// Assigns canonical codes to code lengths (symbols sorted by length, then by
// symbol; code_{i+1} = (code_i + 1) << (len_{i+1} - len_i)), matching the
// decoder's table construction.
static void BuildCanonical(const int* const lengths, int n, uint32_t* codes) {
  int len, s;
  uint32_t code = 0;
  for (len = 1; len <= 15; ++len) {
    for (s = 0; s < n; ++s) {
      if (lengths[s] == len) codes[s] = code++;
    }
    code <<= 1;
  }
}

// A trivial (single-symbol) Huffman code: simple code, one 1-bit symbol 0.
static void PutTrivialTree(BitWriter* const bw) {
  PutBits(bw, 1, 1);  // simple code
  PutBits(bw, 0, 1);  // num_symbols - 1 == 0
  PutBits(bw, 0, 1);  // first symbol is 1 bit long
  PutBits(bw, 0, 1);  // symbol 0
}

static const uint8_t kOrder[19] = {
    17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

// Emits 'lengths[0..n)' as a non-simple (code-length coded) Huffman code.
static void PutTree(BitWriter* const bw, const int* const lengths, int n) {
  // RLE the length array into code-length symbols 0..18.
  // tok[i]: symbol, extra-bit count, extra-bit value.
  int tok[3 * 2048][3];
  int ntok = 0, i = 0;
  int cl_lengths[19];
  uint32_t cl_codes[19];
  int used[19], nused = 0, b, k, s, pos, last_pos = 0;

  while (i < n) {
    const int v = lengths[i];
    int run = 1;
    while (i + run < n && lengths[i + run] == v) ++run;
    if (v == 0) {
      int left = run;
      while (left > 0) {
        if (left >= 11) {
          int take = (left > 138) ? 138 : left;
          if (left - take == 1 || left - take == 2) take -= (left - take);
          tok[ntok][0] = 18; tok[ntok][1] = 7; tok[ntok][2] = take - 11;
          ++ntok; left -= take;
        } else if (left >= 3) {
          tok[ntok][0] = 17; tok[ntok][1] = 3; tok[ntok][2] = left - 3;
          ++ntok; left = 0;
        } else {
          tok[ntok][0] = 0; tok[ntok][1] = 0; tok[ntok][2] = 0;
          ++ntok; --left;
        }
      }
    } else {
      int left = run;
      tok[ntok][0] = v; tok[ntok][1] = 0; tok[ntok][2] = 0;
      ++ntok; --left;
      while (left >= 3) {
        const int take = (left > 6) ? 6 : left;
        tok[ntok][0] = 16; tok[ntok][1] = 2; tok[ntok][2] = take - 3;
        ++ntok; left -= take;
      }
      while (left-- > 0) {
        tok[ntok][0] = v; tok[ntok][1] = 0; tok[ntok][2] = 0;
        ++ntok;
      }
    }
    i += run;
  }

  // Build the code-length code: a complete code over the used symbols with
  // lengths b or b+1 (2^b - k symbols get b, 2k get b+1).
  {
    int freq[19] = {0};
    for (i = 0; i < ntok; ++i) freq[tok[i][0]] = 1;
    for (s = 0; s < 19; ++s) {
      if (freq[s]) used[nused++] = s;
    }
    memset(cl_lengths, 0, sizeof(cl_lengths));
    if (nused == 1) {
      cl_lengths[used[0]] = 1;
    } else {
      b = 0;
      while ((1 << (b + 1)) <= nused) ++b;
      k = nused - (1 << b);
      for (s = 0; s < nused; ++s) {
        cl_lengths[used[s]] = (s < (1 << b) - k) ? b : b + 1;
      }
    }
    BuildCanonical(cl_lengths, 19, cl_codes);
    for (s = 0; s < 19; ++s) {
      if (cl_lengths[s] > 0) {
        for (pos = 0; pos < 19 && kOrder[pos] != s; ++pos) {}
        if (pos + 1 > last_pos) last_pos = pos + 1;
      }
    }
    if (last_pos < 4) last_pos = 4;
  }

  PutBits(bw, 0, 1);                 // not a simple code
  PutBits(bw, (uint32_t)(last_pos - 4), 4);  // num_codes - 4
  for (pos = 0; pos < last_pos; ++pos) {
    PutBits(bw, (uint32_t)cl_lengths[kOrder[pos]], 3);
  }
  PutBits(bw, 0, 1);                 // no explicit length: read n symbols
  for (i = 0; i < ntok; ++i) {
    PutSymbol(bw, cl_codes[tok[i][0]], cl_lengths[tok[i][0]]);
    if (tok[i][1] > 0) PutBits(bw, (uint32_t)tok[i][2], tok[i][1]);
  }
}

//------------------------------------------------------------------------------

// The VP8L chunk payload (everything after the 0x2f signature byte).
static int BuildVp8lBody(uint8_t* const out) {
  BitWriter bw;
  int green_lengths[280];
  uint32_t green_codes[280];
  int i, s;

  memset(&bw, 0, sizeof(bw));
  memset(green_lengths, 0, sizeof(green_lengths));

  // Image size: MAIN_W x MAIN_H, no alpha hint, version 0.
  PutBits(&bw, MAIN_W - 1, 14);
  PutBits(&bw, MAIN_H - 1, 14);
  PutBits(&bw, 0, 1);
  PutBits(&bw, 0, 3);

  // Level-0 image stream.
  PutBits(&bw, 0, 1);  // no transform
  PutBits(&bw, 0, 1);  // no color cache

  // Meta-Huffman codes: the Huffman image is HXSIZE x HYSIZE.
  PutBits(&bw, 1, 1);                       // use meta Huffman codes
  PutBits(&bw, HPREC - 2, 3);               // subsampling precision

  // Huffman image sub-stream: GREEN carries the group index per pixel.
  // Symbols {0..200, 204} get a complete 7/8-bit code.
  for (s = 0; s <= 53; ++s) green_lengths[s] = 7;
  for (s = 54; s <= 200; ++s) green_lengths[s] = 8;
  green_lengths[TOP_GROUP] = 8;
  BuildCanonical(green_lengths, 280, green_codes);

  PutBits(&bw, 0, 1);  // sub-stream: no color cache
  PutTree(&bw, green_lengths, 280);         // GREEN
  PutTrivialTree(&bw);                      // RED
  PutTrivialTree(&bw);                      // BLUE
  PutTrivialTree(&bw);                      // ALPHA
  PutTrivialTree(&bw);                      // DIST

  // Sub-stream pixels: HPIX entries listing every used group index
  // (0..200 and 204), then filler.
  for (i = 0; i < HPIX; ++i) {
    if (i <= 200) {
      s = i;
    } else if (i == 201) {
      s = TOP_GROUP;
    } else {
      s = 0;
    }
    PutSymbol(&bw, green_codes[s], green_lengths[s]);
  }

  // Main stream Huffman codes: one trivial five-tree group per index
  // (the helper reads a group for every index below GROUP_MAX).
  for (i = 0; i < GROUP_MAX; ++i) {
    PutTrivialTree(&bw);  // GREEN
    PutTrivialTree(&bw);  // RED
    PutTrivialTree(&bw);  // BLUE
    PutTrivialTree(&bw);  // ALPHA
    PutTrivialTree(&bw);  // DIST
  }

  // Level-0 pixel data: every group is a trivial code, so pixels consume no
  // bits; pad the payload instead.
  PutBits(&bw, 0, 64 * 8);

  memcpy(out, bw.bytes, sizeof(bw.bytes));
  return (bw.bit_pos + 7) >> 3;
}

//------------------------------------------------------------------------------

static void PutTag(uint8_t* const p, const char* tag) {
  memcpy(p, tag, 4);
}

static void PutLE32(uint8_t* const p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

int main(void) {
  uint8_t body[2048];
  int body_size;
  size_t vp8l_size, p = 0;
  uint8_t webp[4096];
  uint8_t* rgba;
  int width = 0, height = 0;

  body_size = BuildVp8lBody(body);
  vp8l_size = (size_t)body_size + 1;  // 1 signature byte

  PutTag(webp + p, "RIFF"); p += 4;
  PutLE32(webp + p, (uint32_t)(4 + 8 + vp8l_size)); p += 4;
  PutTag(webp + p, "WEBP"); p += 4;
  PutTag(webp + p, "VP8L"); p += 4;
  PutLE32(webp + p, (uint32_t)vp8l_size); p += 4;
  webp[p++] = 0x2f;  // VP8L signature
  memcpy(webp + p, body, body_size); p += (size_t)body_size;

  rgba = WebPDecodeRGBA(webp, p, &width, &height);
  printf("decoded %p (%dx%d)\n", (void*)rgba, width, height);
  if (rgba != NULL) {
    // Touch the pixels so a silent decode cannot be optimized away.
    const size_t n = (size_t)width * height * 4;
    size_t i, sum = 0;
    for (i = 0; i < n; ++i) sum += rgba[i];
    printf("checksum %u\n", (unsigned)sum);
    WebPFree(rgba);
  }
  return 0;
}
