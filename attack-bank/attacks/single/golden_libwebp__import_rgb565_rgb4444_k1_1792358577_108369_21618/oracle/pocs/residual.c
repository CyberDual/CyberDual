#include <stdlib.h>

#include "extras/extras.h"
#include "src/webp/types.h"
#include "src/webp/decode.h"
#include "src/webp/encode.h"

int main(void) {
  const int width = 1;
  const int height = 1;
  uint8_t* rgb4444 = malloc(2u * width * height);
  WebPPicture picture;
  WebPConfig config;
  WebPMemoryWriter writer;
  uint8_t* decoded;
  int decoded_width;
  int decoded_height;

  if (rgb4444 == NULL) return 1;
  rgb4444[0] = 0x12;
  rgb4444[1] = 0x34;
  if (!WebPPictureInit(&picture)) return 2;
  picture.width = width;
  picture.height = height;
  if (!WebPImportRGB4444(rgb4444, &picture)) return 3;
  if (!WebPConfigInit(&config)) return 4;
  config.lossless = 1;
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(&config, &picture)) return 5;
  decoded = WebPDecodeRGBA(writer.mem, writer.size, &decoded_width,
                           &decoded_height);
  free(decoded);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&picture);
  free(rgb4444);
  return decoded == NULL || decoded_width != width || decoded_height != height;
}
