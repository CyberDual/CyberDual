// Single-frame animation with mixed compression, then assembled.
#include <stdio.h>
#include <stdlib.h>

#include "webp/encode.h"
#include "webp/mux.h"

static uint32_t NextRandom(uint32_t* const state) {
  *state = *state * 1103515245u + 12345u;
  return (*state >> 16) & 0xff;
}

int main(void) {
  const int kWidth = 96;
  const int kHeight = 96;
  WebPAnimEncoderOptions options;
  WebPAnimEncoder* enc;
  WebPConfig config;
  WebPPicture pic;
  WebPData webp_data;
  uint32_t state = 1;
  int x, y;

  WebPDataInit(&webp_data);

  if (!WebPAnimEncoderOptionsInit(&options)) return 1;
  options.allow_mixed = 1;

  enc = WebPAnimEncoderNew(kWidth, kHeight, &options);
  if (enc == NULL) return 1;

  if (!WebPConfigInit(&config)) return 1;
  config.lossless = 1;

  if (!WebPPictureInit(&pic)) return 1;
  pic.width = kWidth;
  pic.height = kHeight;
  pic.use_argb = 1;
  if (!WebPPictureAlloc(&pic)) return 1;
  for (y = 2; y < kHeight - 2; ++y) {
    for (x = 2; x < kWidth - 2; ++x) {
      const uint32_t r = NextRandom(&state);
      const uint32_t g = NextRandom(&state);
      const uint32_t b = NextRandom(&state);
      pic.argb[y * pic.argb_stride + x] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
  }

  if (!WebPAnimEncoderAdd(enc, &pic, 0, &config)) return 1;
  if (!WebPAnimEncoderAdd(enc, NULL, 100, NULL)) return 1;
  if (!WebPAnimEncoderAssemble(enc, &webp_data)) return 1;

  printf("assembled %zu bytes\n", webp_data.size);

  WebPDataClear(&webp_data);
  WebPPictureFree(&pic);
  WebPAnimEncoderDelete(enc);
  return 0;
}
