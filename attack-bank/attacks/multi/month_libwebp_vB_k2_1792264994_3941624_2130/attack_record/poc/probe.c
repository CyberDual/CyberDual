#include <stdint.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"

static void FillTile(uint8_t* rgba, int width, int height, int tile_x,
                     int tile_y, int tile_width, int tile_height, int kind) {
  const int base_x = tile_x * tile_width;
  const int base_y = tile_y * tile_height;
  for (int y = 0; y < tile_height; ++y) {
    for (int x = 0; x < tile_width; ++x) {
      uint8_t* pixel = rgba + ((base_y + y) * width + base_x + x) * 4;
      switch (kind) {
        case 0:
          pixel[0] = (uint8_t)(x * 255 / tile_width);
          pixel[1] = 0;
          pixel[2] = (uint8_t)(y * 255 / tile_height);
          pixel[3] = 255;
          break;
        case 1:
          pixel[0] = (uint8_t)(x ^ y ^ (x * y));
          pixel[1] = (uint8_t)((x + y) * 3);
          pixel[2] = (uint8_t)((x * y) ^ (x + y));
          pixel[3] = 255;
          break;
        case 2:
          pixel[0] = 255;
          pixel[1] = 165;
          pixel[2] = 0;
          pixel[3] = 255;
          break;
        case 3:
          pixel[0] = ((x / 8 + y / 8) & 1) ? 255 : 0;
          pixel[1] = ((x / 8 + y / 8) & 1) ? 255 : 0;
          pixel[2] = ((x / 8 + y / 8) & 1) ? 255 : 0;
          pixel[3] = 255;
          break;
        case 4:
          pixel[0] = 255;
          pixel[1] = (uint8_t)(x * 255 / tile_width);
          pixel[2] = 0;
          pixel[3] = 255;
          break;
        case 5:
          pixel[0] = (uint8_t)(x * 7 + y * 13);
          pixel[1] = (uint8_t)(x * 11 + y * 5);
          pixel[2] = (uint8_t)(x * 3 + y * 17);
          pixel[3] = 255;
          break;
        case 6:
          pixel[0] = 255;
          pixel[1] = 0;
          pixel[2] = 255;
          pixel[3] = 255;
          break;
        default:
          pixel[0] = (uint8_t)(((x + y) / 8) & 1 ? 255 : 0);
          pixel[1] = (uint8_t)(((x - y + tile_width) / 8) & 1 ? 255 : 0);
          pixel[2] = (uint8_t)(((x / 4 + y / 4) & 1) ? 255 : 0);
          pixel[3] = 255;
          break;
      }
    }
  }
}

int main(void) {
  const int width = 384;
  const int height = 384;
  const int tile_width = width / 4;
  const int tile_height = height / 4;
  uint8_t* rgba = (uint8_t*)malloc((size_t)width * height * 4);
  uint8_t* webp_data = NULL;
  size_t webp_size;
  int decoded_width = 0;
  int decoded_height = 0;
  uint8_t* decoded = NULL;

  if (rgba == NULL) return 1;

  for (int tile_y = 0; tile_y < 4; ++tile_y) {
    for (int tile_x = 0; tile_x < 4; ++tile_x) {
      const int kind = (tile_y * 4 + tile_x) % 8;
      FillTile(rgba, width, height, tile_x, tile_y, tile_width, tile_height,
               kind);
    }
  }

  webp_size = WebPEncodeLosslessRGBA(rgba, width, height, width * 4,
                                     &webp_data);
  free(rgba);
  if (webp_size == 0) return 2;

  decoded = WebPDecodeRGBA(webp_data, webp_size, &decoded_width,
                           &decoded_height);
  free(webp_data);
  free(decoded);
  return decoded == NULL ? 3 : 0;
}
