// Self-contained probe: builds a lossy WebP with an uncompressed alpha
// channel via the public encoder, then decodes it back through the public
// decoder API while requesting alpha dithering (a documented decoding
// option). Any sanitizer abort must come from inside libwebp.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <webp/decode.h>
#include <webp/encode.h>

#define WIDTH 1000
#define HEIGHT 1000

int main(void) {
  const int width = WIDTH;
  const int height = HEIGHT;
  uint8_t* rgba = (uint8_t*)malloc((size_t)width * height * 4);
  int x, y;

  WebPConfig config;
  WebPPicture picture;
  WebPMemoryWriter writer;
  WebPDecoderConfig dconfig;
  VP8StatusCode status;

  if (rgba == NULL) return 1;
  // A simple gradient with varying, non-opaque alpha so the encoder keeps
  // the alpha channel.
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      uint8_t* const p = rgba + 4 * ((size_t)y * width + x);
      p[0] = (uint8_t)(x % 256);
      p[1] = (uint8_t)(y % 256);
      p[2] = (uint8_t)((x + y) % 256);
      p[3] = (uint8_t)((y % 4) * 64);
    }
  }

  // Encode lossy with an uncompressed alpha stream at full quality.
  if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) return 1;
  config.quality = 75;
  config.method = 4;
  config.alpha_compression = 0;  // ALPHA_NO_COMPRESSION
  config.alpha_quality = 100;    // keep levels as-is
  picture.width = width;
  picture.height = height;
  picture.use_argb = 1;
  if (!WebPPictureImportRGBA(&picture, rgba, width * 4)) return 1;
  WebPMemoryWriterInit(&writer);
  picture.writer = WebPMemoryWrite;
  picture.custom_ptr = &writer;
  if (!WebPEncode(&config, &picture)) {
    fprintf(stderr, "encode failed: %d\n", (int)picture.error_code);
    return 1;
  }
  WebPPictureFree(&picture);
  free(rgba);

  // Decode back, requesting alpha dithering via the public decoding options.
  if (!WebPInitDecoderConfig(&dconfig)) return 1;
  if (WebPGetFeatures(writer.mem, writer.size, &dconfig.input) !=
      VP8_STATUS_OK) {
    return 1;
  }
  dconfig.output.colorspace = MODE_RGBA;
  dconfig.options.alpha_dithering_strength = 100;
  status = WebPDecode(writer.mem, writer.size, &dconfig);
  if (status != VP8_STATUS_OK) {
    fprintf(stderr, "decode failed: %d\n", (int)status);
    WebPFreeDecBuffer(&dconfig.output);
    WebPMemoryWriterClear(&writer);
    return 1;
  }

  printf("decoded %dx%d, stride=%d\n", dconfig.output.width,
         dconfig.output.height, dconfig.output.u.RGBA.stride);
  WebPFreeDecBuffer(&dconfig.output);
  WebPMemoryWriterClear(&writer);
  return 0;
}
