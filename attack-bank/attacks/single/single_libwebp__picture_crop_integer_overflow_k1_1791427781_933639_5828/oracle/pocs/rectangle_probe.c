#include <stdint.h>
#include <stdlib.h>

#include "src/webp/decode.h"
#include "src/webp/encode.h"

int main(void) {
  uint8_t rgba[32 * 32 * 4];
  uint8_t* encoded = NULL;
  uint8_t* decoded;
  size_t encoded_size;
  WebPPicture picture;
  int width, height;
  int i;

  for (i = 0; i < 32 * 32; ++i) {
    rgba[4 * i + 0] = (uint8_t)i;
    rgba[4 * i + 1] = (uint8_t)(i >> 2);
    rgba[4 * i + 2] = (uint8_t)(i >> 4);
    rgba[4 * i + 3] = 255;
  }
  encoded_size = WebPEncodeLosslessRGBA(rgba, 32, 32, 32 * 4, &encoded);
  if (encoded_size == 0) return 1;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != 32 || height != 32) return 2;
  if (!WebPPictureInit(&picture)) return 3;
  picture.width = width;
  picture.height = height;
  if (!WebPPictureImportRGBA(&picture, decoded, width * 4)) return 4;
  WebPFree(decoded);
  if (!WebPPictureCrop(&picture, 0, 28, 32, 4)) return 5;
  WebPPictureFree(&picture);
  return 0;
}
