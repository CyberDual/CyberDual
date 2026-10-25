#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "webp/decode.h"
#include "webp/encode.h"

int main(void) {
  uint8_t rgba[8] = {0xff, 0, 0, 0xff, 0, 0xff, 0, 0xff};
  uint8_t* encoded = NULL;
  uint8_t* decoded = NULL;
  size_t encoded_size;
  int decoded_width;
  int decoded_height;
  WebPPicture picture;

  encoded_size = WebPEncodeRGBA(rgba, 2, 1, 8, 100.f, &encoded);
  if (encoded_size == 0) return 1;
  decoded = WebPDecodeRGBA(encoded, encoded_size, &decoded_width,
                           &decoded_height);
  WebPFree(encoded);
  if (decoded == NULL || decoded_width != 2 || decoded_height != 1) {
    WebPFree(decoded);
    return 1;
  }

  if (!WebPPictureInit(&picture)) return 1;
  picture.width = 2;
  picture.height = 1;
  picture.use_argb = 1;
  if (!WebPPictureImportRGBA(&picture, decoded, 8)) {
    WebPFree(decoded);
    WebPPictureFree(&picture);
    return 1;
  }
  WebPFree(decoded);

  if (!WebPPictureCrop(&picture, 0, INT_MAX - 1, 2, 4)) {
    WebPPictureFree(&picture);
    return 2;
  }

  WebPPictureFree(&picture);
  return 0;
}
