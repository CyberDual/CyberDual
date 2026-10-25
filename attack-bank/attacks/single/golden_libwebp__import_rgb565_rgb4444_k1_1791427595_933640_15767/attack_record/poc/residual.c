#include <stdint.h>
#include <stdlib.h>

#include "extras/extras.h"
#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  enum { kWidth = 3, kHeight = 2 };
  const uint8_t rgba[kWidth * kHeight * 4] = {
      240, 32, 16, 255, 32, 224, 80, 255, 0, 0, 255, 255,
      255, 255, 255, 255, 120, 48, 200, 255, 16, 144, 40, 255};
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  uint8_t* rgb565;
  size_t encoded_size;
  int width, height, i;
  WebPPicture picture;

  encoded_size = WebPEncodeLosslessRGBA(rgba, kWidth, kHeight, 4 * kWidth,
                                       &encoded);
  if (encoded_size == 0) return 1;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != kWidth || height != kHeight) return 2;

  rgb565 = (uint8_t*)malloc((size_t)width * height * 2);
  if (rgb565 == NULL) return 3;
  for (i = 0; i < width * height; ++i) {
    const uint16_t value = (uint16_t)(((decoded[4 * i] >> 3) << 11) |
                                      ((decoded[4 * i + 1] >> 2) << 5) |
                                      (decoded[4 * i + 2] >> 3));
    rgb565[2 * i] = (uint8_t)(value >> 8);
    rgb565[2 * i + 1] = (uint8_t)value;
  }
  WebPFree(decoded);

  if (!WebPPictureInit(&picture)) return 4;
  picture.width = width;
  picture.height = height;
  if (!WebPImportRGB565(rgb565, &picture)) return 5;
  WebPPictureFree(&picture);
  free(rgb565);
  return 0;
}
