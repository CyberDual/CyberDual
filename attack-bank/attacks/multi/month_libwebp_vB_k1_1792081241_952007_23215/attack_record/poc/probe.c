// Probe: decode a lossy WebP with an uncompressed alpha channel while
// alpha dithering is requested. Uses only the public libwebp API.
//
// The image is produced with the public encoder (WebPEncodeRGB) and wrapped
// into a VP8X/ALPH/VP8 container at runtime; the ALPH header selects the
// "no compression / no filter / no level pre-processing" mode, which is a
// fully valid stream.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

#define IMG_WIDTH 64
#define IMG_HEIGHT 1024

static void PutLE32(uint8_t* const p, uint32_t v) {
  p[0] = (uint8_t)(v >> 0);
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static void PutLE24(uint8_t* const p, uint32_t v) {
  p[0] = (uint8_t)(v >> 0);
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
}

static void PutTag(uint8_t* const p, const char* tag) {
  p[0] = (uint8_t)tag[0];
  p[1] = (uint8_t)tag[1];
  p[2] = (uint8_t)tag[2];
  p[3] = (uint8_t)tag[3];
}

// Returns the payload of the first chunk with tag 'tag' in 'buf', or NULL.
static const uint8_t* FindChunk(const uint8_t* const buf, size_t buf_size,
                                const char* tag, size_t* const payload_size) {
  size_t pos = 12;  // Skip "RIFFnnnnWEBP".
  while (pos + 8 <= buf_size) {
    const uint32_t size =
        (uint32_t)buf[pos + 4] | ((uint32_t)buf[pos + 5] << 8) |
        ((uint32_t)buf[pos + 6] << 16) | ((uint32_t)buf[pos + 7] << 24);
    if (pos + 8 + size > buf_size) return NULL;
    if (!memcmp(buf + pos, tag, 4)) {
      *payload_size = size;
      return buf + pos + 8;
    }
    pos += 8 + size + (size & 1);
  }
  return NULL;
}

int main(void) {
  // Encode a lossy (VP8) key frame of a gradient image.
  const size_t rgb_size = (size_t)IMG_WIDTH * IMG_HEIGHT * 3;
  uint8_t* const rgb = (uint8_t*)malloc(rgb_size);
  uint8_t* encoded = NULL;
  size_t encoded_size, vp8_size, alph_size, disk_alph, disk_vp8, file_size;
  const uint8_t* vp8_payload;
  uint8_t* file;
  size_t pos;
  int x, y;
  WebPDecoderConfig config;
  VP8StatusCode status;

  if (rgb == NULL) return 1;
  for (y = 0; y < IMG_HEIGHT; ++y) {
    for (x = 0; x < IMG_WIDTH; ++x) {
      uint8_t* const px =
          rgb + 3 * ((size_t)y * IMG_WIDTH + x);
      px[0] = (uint8_t)(x * 4);
      px[1] = (uint8_t)(y & 0xff);
      px[2] = (uint8_t)((x ^ y) & 0xff);
    }
  }
  encoded_size = WebPEncodeRGB(rgb, IMG_WIDTH, IMG_HEIGHT, IMG_WIDTH * 3,
                               80.0f, &encoded);
  free(rgb);
  if (encoded == NULL || encoded_size == 0) return 1;

  vp8_payload = FindChunk(encoded, encoded_size, "VP8 ", &vp8_size);
  if (vp8_payload == NULL || vp8_size == 0) {
    WebPFree(encoded);
    return 1;
  }

  // Rebuild the stream as: VP8X + ALPH (uncompressed, not level
  // pre-processed) + VP8.
  alph_size = 1 + (size_t)IMG_WIDTH * IMG_HEIGHT;  // header byte + plane
  disk_alph = alph_size + (alph_size & 1);
  disk_vp8 = vp8_size + (vp8_size & 1);
  file_size = 4 /* "WEBP" */ + 8 + 10 /* VP8X */ + 8 + disk_alph /* ALPH */ +
              8 + disk_vp8 /* VP8 */;
  file = (uint8_t*)malloc(8 + file_size);
  if (file == NULL) {
    WebPFree(encoded);
    return 1;
  }
  PutTag(file, "RIFF");
  PutLE32(file + 4, (uint32_t)file_size);
  PutTag(file + 8, "WEBP");
  pos = 12;

  PutTag(file + pos, "VP8X");
  PutLE32(file + pos + 4, 10);
  PutLE32(file + pos + 8, 0x10);  // ALPHA_FLAG
  PutLE24(file + pos + 12, IMG_WIDTH - 1);
  PutLE24(file + pos + 15, IMG_HEIGHT - 1);
  pos += 8 + 10;

  PutTag(file + pos, "ALPH");
  PutLE32(file + pos + 4, (uint32_t)alph_size);
  file[pos + 8] = 0;  // method=0 (none), filter=0 (none), pre-processing=0
  for (y = 0; y < IMG_HEIGHT; ++y) {
    uint8_t* const dst = file + pos + 9 + (size_t)y * IMG_WIDTH;
    for (x = 0; x < IMG_WIDTH; ++x) {
      dst[x] = (uint8_t)(x * 3 + y * 5);
    }
  }
  if (alph_size & 1) file[pos + 8 + alph_size] = 0;
  pos += 8 + disk_alph;

  PutTag(file + pos, "VP8 ");
  PutLE32(file + pos + 4, (uint32_t)vp8_size);
  memcpy(file + pos + 8, vp8_payload, vp8_size);
  if (vp8_size & 1) file[pos + 8 + vp8_size] = 0;
  pos += 8 + disk_vp8;
  WebPFree(encoded);

  if (pos != 8 + file_size) {
    free(file);
    return 1;
  }

  // Decode it back, requesting alpha dithering (a documented 0..100 option).
  if (!WebPInitDecoderConfig(&config)) {
    free(file);
    return 1;
  }
  config.options.alpha_dithering_strength = 100;
  config.output.colorspace = MODE_RGBA;
  status = WebPDecode(file, pos, &config);
  printf("decode status: %d, %dx%d\n", (int)status, config.input.width,
         config.input.height);
  WebPFreeDecBuffer(&config.output);
  free(file);
  return (status == VP8_STATUS_OK) ? 0 : 1;
}
