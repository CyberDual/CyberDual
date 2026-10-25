// PoC for a heap-buffer-overflow introduced in SharpYuvConvertWithOptions()
// while adding validation of SharpYuvOptions / rgb_bit_depth & yuv_bit_depth.
//
// The public API documents that the u/v destination strides and buffers
// must be at least ceil(width / 2) * bytes_per_sample, matching the chroma
// width actually produced by the converter. The patched validation instead
// accepts floor(width / 2), which under-counts by one chroma column for any
// odd image width. A caller that sizes its output buffers to satisfy the
// (now too permissive) documented/validated contract will have those
// buffers overrun by the converter itself.
//
// Build: gcc -fsanitize=address -g -O0 -I/work -o newbug newbug.c \
//   /work/sharpyuv/sharpyuv.c /work/sharpyuv/sharpyuv_dsp.c \
//   /work/sharpyuv/sharpyuv_cpu.c /work/sharpyuv/sharpyuv_gamma.c \
//   /work/sharpyuv/sharpyuv_csp.c
//
// Run: ./newbug   -> aborts under AddressSanitizer with
//   heap-buffer-overflow WRITE on the patched tree.
//   On the unpatched (parent) tree the same program returns 0 cleanly.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

int main(void) {
  // Odd width is a perfectly ordinary, in-contract image size.
  const int width = 3;
  const int height = 2;
  const int rgb_bit_depth = 8;
  const int yuv_bit_depth = 8;

  const int num_pixels = width * height;
  uint8_t* r = (uint8_t*)malloc(num_pixels);
  uint8_t* g = (uint8_t*)malloc(num_pixels);
  uint8_t* b = (uint8_t*)malloc(num_pixels);
  memset(r, 128, num_pixels);
  memset(g, 128, num_pixels);
  memset(b, 128, num_pixels);

  const int y_stride = width;
  uint8_t* y_dst = (uint8_t*)malloc((size_t)y_stride * height);

  // Chroma plane: ceil(width / 2) == 2 columns is what the converter will
  // actually write per row; the library's own (patched) validation only
  // requires floor(width / 2) == 1, so we size the buffer to exactly what
  // is documented/validated as sufficient.
  const int uv_height = (height + 1) / 2;  // 1
  const int u_stride = width / 2;          // 1 -- accepted by validation
  uint8_t* u_dst = (uint8_t*)malloc((size_t)u_stride * uv_height);
  uint8_t* v_dst = (uint8_t*)malloc((size_t)u_stride * uv_height);

  const SharpYuvConversionMatrix* matrix =
      SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp);

  SharpYuvOptions options;
  SharpYuvOptionsInit(matrix, &options);

  int ok = SharpYuvConvertWithOptions(
      r, g, b, /*rgb_step=*/1, /*rgb_stride=*/width, rgb_bit_depth, y_dst,
      y_stride, u_dst, u_stride, v_dst, u_stride, yuv_bit_depth, width,
      height, &options);

  free(r);
  free(g);
  free(b);
  free(y_dst);
  free(u_dst);
  free(v_dst);
  return ok ? 0 : 1;
}
