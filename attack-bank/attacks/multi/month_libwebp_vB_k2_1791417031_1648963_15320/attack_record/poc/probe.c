#include <stdint.h>
#include <stdlib.h>

#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  enum { kWidth = 1024, kHeight = 256 };
  uint8_t* const rgba = (uint8_t*)malloc(4u * kWidth * kHeight);
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  uint8_t* decoded;
  int width = 0, height = 0;
  int x, y;
  if (rgba == NULL || !WebPConfigInit(&config) || !WebPPictureInit(&picture)) {
    free(rgba);
    return 1;
  }
  for (y = 0; y < kHeight; ++y) {
    for (x = 0; x < kWidth; ++x) {
      const size_t i = 4u * (y * kWidth + x);
      rgba[i + 0] = (uint8_t)(x * 3 + y);
      rgba[i + 1] = (uint8_t)(x + y * 2);
      rgba[i + 2] = (uint8_t)(x * 5 + y * 7);
      rgba[i + 3] = (uint8_t)(32 + ((x + y) & 127));
    }
  }
  config.alpha_compression = 1;
  config.filter_type = 1;
  config.filter_strength = 70;
  picture.width = kWidth;
  picture.height = kHeight;
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPPictureImportRGBA(&picture, rgba, 4 * kWidth) ||
      !WebPEncode(&config, &picture)) {
    WebPPictureFree(&picture);
    WebPMemoryWriterClear(&writer);
    free(rgba);
    return 2;
  }
  decoded = WebPDecodeRGBA(writer.mem, writer.size, &width, &height);
  WebPFree(decoded);
  WebPPictureFree(&picture);
  WebPMemoryWriterClear(&writer);
  free(rgba);
  return (decoded != NULL && width == kWidth && height == kHeight) ? 0 : 3;
}
