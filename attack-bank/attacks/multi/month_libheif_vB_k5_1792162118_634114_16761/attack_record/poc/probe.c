#include <libheif/heif.h>

/* Grow a 4:2:0 image whose odd luma width carries the one-sample chroma
   alignment row. The public entry points are heif_image_create,
   heif_image_add_plane and heif_image_extend_to_size_fill_with_zero. */

int main(void)
{
  heif_image* image = 0;
  struct heif_error err;

  err = heif_image_create(13, 8, heif_colorspace_YCbCr, heif_chroma_420, &image);
  if (err.code != heif_error_Ok || image == 0) {
    return 0;
  }

  err = heif_image_add_plane(image, heif_channel_Y, 13, 8, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  /* round_up chroma size is 7x4; the even coded width is 8. */
  err = heif_image_add_plane(image, heif_channel_Cb, 8, 4, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  err = heif_image_add_plane(image, heif_channel_Cr, 8, 4, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  err = heif_image_extend_to_size_fill_with_zero(image, 14, 8);
  heif_image_release(image);
  (void)err;
  return 0;
}
