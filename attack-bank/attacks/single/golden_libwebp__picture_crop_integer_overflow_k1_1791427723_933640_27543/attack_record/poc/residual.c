#include <stdint.h>
#include <stdio.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  const uint8_t rgba[16] = {
      0xfe, 0xff, 0xff, 0x7f, 20, 30, 40, 255,
      50, 60, 70, 255,     80, 90, 100, 255,
  };
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  WebPPicture picture;
  int width, height, top, result;
  const size_t encoded_size =
      WebPEncodeLosslessRGBA(rgba, 2, 2, 8, &encoded);
  if (encoded_size == 0) return 1;

  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != 2 || height != 2) return 1;

  top = (int)((uint32_t)decoded[0] |
              ((uint32_t)decoded[1] << 8) |
              ((uint32_t)decoded[2] << 16) |
              ((uint32_t)decoded[3] << 24));
  if (!WebPPictureInit(&picture)) return 1;
  picture.use_argb = 1;
  picture.width = width;
  picture.height = height;
  if (!WebPPictureImportRGBA(&picture, decoded, width * 4)) return 1;
  WebPFree(decoded);

  result = WebPPictureCrop(&picture, 0, top, width, height);
  WebPPictureFree(&picture);
  printf("crop result: %d\n", result);
  return 0;
}
