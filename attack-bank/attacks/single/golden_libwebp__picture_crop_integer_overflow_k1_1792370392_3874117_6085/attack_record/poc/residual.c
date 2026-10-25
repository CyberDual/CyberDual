// End-to-end probe over the public libwebp API: encode a small RGBA image,
// decode it back, import it into a WebPPicture, convert to planar YUV(A) and
// crop the picture at an extreme top coordinate (all other parameters are
// ordinary, in-bounds values).
//
// Build: clang -fsanitize=address -g -O1 -I<src root>/src residual.c \
//            <libwebp.a> <libsharpyuv.a> -lm -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#define SRC_WIDTH 8
#define SRC_HEIGHT 8

int main(void) {
  uint8_t rgba[SRC_WIDTH * SRC_HEIGHT * 4];
  uint8_t* encoded = NULL;
  uint8_t* decoded = NULL;
  int width = 0, height = 0, i;
  size_t size;
  WebPPicture pic;

  for (i = 0; i < SRC_WIDTH * SRC_HEIGHT * 4; ++i) {
    rgba[i] = (uint8_t)(i * 37 + 11);
  }

  size = WebPEncodeRGBA(rgba, SRC_WIDTH, SRC_HEIGHT, SRC_WIDTH * 4, 75.f,
                        &encoded);
  if (size == 0) {
    fprintf(stderr, "encode failed\n");
    return 1;
  }

  decoded = WebPDecodeRGBA(encoded, (int)size, &width, &height);
  WebPFree(encoded);
  if (decoded == NULL || width != SRC_WIDTH || height != SRC_HEIGHT) {
    fprintf(stderr, "decode failed\n");
    WebPFree(decoded);
    return 1;
  }
  printf("decoded %dx%d\n", width, height);

  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "picture init failed\n");
    WebPFree(decoded);
    return 1;
  }
  pic.width = width;
  pic.height = height;
  if (!WebPPictureImportRGBA(&pic, decoded, width * 4)) {
    fprintf(stderr, "import failed\n");
    WebPPictureFree(&pic);
    WebPFree(decoded);
    return 1;
  }
  WebPFree(decoded);

  // 'pic' now holds planar YUV(A) buffers (y/u/v planes, y_stride == width).

  // Crop the whole picture, but positioned at a huge top coordinate.
  if (!WebPPictureCrop(&pic, 0, 2147483646, width, height)) {
    printf("crop rejected (error %d)\n", pic.error_code);
    WebPPictureFree(&pic);
    return 0;
  }

  printf("crop succeeded: %dx%d\n", pic.width, pic.height);
  WebPPictureFree(&pic);
  return 0;
}
