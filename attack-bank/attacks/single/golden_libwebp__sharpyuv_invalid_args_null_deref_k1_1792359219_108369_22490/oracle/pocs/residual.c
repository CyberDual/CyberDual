#include <stddef.h>
#include <stdint.h>

#include "src/webp/decode.h"
#include "src/webp/encode.h"

int main(void) {
  static const uint8_t rgba[64] = {
      244, 67, 54, 255,  33, 150, 243, 255,
      76, 175, 80, 255, 255, 235, 59, 255,
      63, 81, 181, 255,   0, 188, 212, 255,
      3, 169, 244, 255, 156, 39, 176, 255,
      244, 67, 54, 255,  33, 150, 243, 255,
      76, 175, 80, 255, 255, 235, 59, 255,
      63, 81, 181, 255,   0, 188, 212, 255,
      3, 169, 244, 255, 156, 39, 176, 255,
  };
  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  uint8_t* decoded;
  int width;
  int height;

  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) {
    return 1;
  }
  config.quality = 90;
  config.use_sharp_yuv = 1;
  picture.use_argb = 1;
  picture.width = 4;
  picture.height = 4;
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  WebPMemoryWriterInit(&writer);

  if (!WebPPictureImportRGBA(&picture, rgba, 4 * 4) ||
      !WebPEncode(&config, &picture)) {
    WebPPictureFree(&picture);
    WebPMemoryWriterClear(&writer);
    return 2;
  }
  WebPPictureFree(&picture);

  decoded = WebPDecodeRGBA(writer.mem, (int)writer.size, &width, &height);
  WebPMemoryWriterClear(&writer);
  if (decoded == NULL || width != 4 || height != 4) {
    WebPFree(decoded);
    return 3;
  }
  WebPFree(decoded);
  return 0;
}
