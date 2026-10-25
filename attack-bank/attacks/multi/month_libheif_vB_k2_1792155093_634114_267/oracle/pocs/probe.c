#include <libheif/heif.h>

int main(void)
{
  heif_image* image = 0;
  struct heif_error err;

  err = heif_image_create(8, 8, heif_colorspace_YCbCr, heif_chroma_444, &image);
  if (err.code != heif_error_Ok || image == 0) {
    return 0;
  }

  err = heif_image_add_plane(image, heif_channel_Y, 8, 8, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }
  err = heif_image_add_plane(image, heif_channel_Cb, 8, 8, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }
  err = heif_image_add_plane(image, heif_channel_Cr, 8, 8, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  err = heif_image_add_plane(image, heif_channel_Alpha, 32, 32, 8);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  err = heif_image_extend_to_size_fill_with_zero(image, 16, 16);

  heif_image_release(image);
  (void)err;
  return 0;
}
