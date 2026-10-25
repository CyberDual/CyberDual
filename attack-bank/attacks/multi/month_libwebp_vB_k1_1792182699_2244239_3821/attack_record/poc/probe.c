// Probe: encode a small RGBA image losslessly, then decode it into an
// external MODE_YUV buffer via the public WebPDecode() API.
//
// On a correct library this decodes successfully (the alpha channel of the
// lossless ARGB data is simply dropped when converting to YUV).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define WIDTH 64
#define HEIGHT 48

int main(void) {
  static uint8_t rgba[WIDTH * HEIGHT * 4];
  uint8_t* encoded = NULL;
  size_t encoded_size = 0;
  int x, y, ok;

  // Deterministic non-trivial image with a few non-opaque pixels.
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      uint8_t* const px = &rgba[4 * (y * WIDTH + x)];
      px[0] = (uint8_t)(x * 13 + y * 7);
      px[1] = (uint8_t)(x * 3 + y * 17);
      px[2] = (uint8_t)((x ^ y) * 11);
      px[3] = 0xff;
    }
  }
  rgba[3] = 0;  // make pixel (0,0) fully transparent

  // Encode losslessly (alpha preserved by construction of VP8L).
  {
    WebPConfig config;
    WebPPicture picture;
    WebPMemoryWriter writer;
    if (!WebPConfigInit(&config)) return 2;
    config.lossless = 1;
    config.exact = 1;
    config.quality = 90.f;
    if (!WebPPictureInit(&picture)) return 2;
    picture.width = WIDTH;
    picture.height = HEIGHT;
    picture.use_argb = 1;
    WebPMemoryWriterInit(&writer);
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    if (!WebPPictureImportRGBA(&picture, rgba, WIDTH * 4)) return 2;
    ok = WebPEncode(&config, &picture);
    WebPPictureFree(&picture);
    if (!ok) return 2;
    encoded = writer.mem;
    encoded_size = writer.size;
  }

  // Decode into a custom external MODE_YUV buffer (in-contract: alpha plane
  // is ignored for MODE_YUV).
  {
    static uint8_t y_plane[WIDTH * HEIGHT];
    static uint8_t u_plane[((WIDTH + 1) / 2) * ((HEIGHT + 1) / 2)];
    static uint8_t v_plane[((WIDTH + 1) / 2) * ((HEIGHT + 1) / 2)];
    WebPDecoderConfig config;
    VP8StatusCode status;
    if (!WebPInitDecoderConfig(&config)) return 2;
    config.output.colorspace = MODE_YUV;
    config.output.is_external_memory = 1;
    config.output.u.YUVA.y = y_plane;
    config.output.u.YUVA.y_stride = WIDTH;
    config.output.u.YUVA.y_size = sizeof(y_plane);
    config.output.u.YUVA.u = u_plane;
    config.output.u.YUVA.u_stride = (WIDTH + 1) / 2;
    config.output.u.YUVA.u_size = sizeof(u_plane);
    config.output.u.YUVA.v = v_plane;
    config.output.u.YUVA.v_stride = (WIDTH + 1) / 2;
    config.output.u.YUVA.v_size = sizeof(v_plane);
    config.output.u.YUVA.a = NULL;
    config.output.u.YUVA.a_stride = 0;
    config.output.u.YUVA.a_size = 0;
    status = WebPDecode(encoded, encoded_size, &config);
    if (status != VP8_STATUS_OK) {
      fprintf(stderr, "decode failed: %d\n", (int)status);
      return 2;
    }
  }

  WebPFree(encoded);
  printf("OK\n");
  return 0;
}
