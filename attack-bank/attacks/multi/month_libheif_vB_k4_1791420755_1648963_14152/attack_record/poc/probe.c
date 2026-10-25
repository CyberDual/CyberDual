#include <libheif/heif.h>

int main(void)
{
  heif_image* image = 0;
  heif_error err = heif_image_create(8, 8, heif_colorspace_monochrome,
                                     heif_chroma_monochrome, &image);
  if (err.code != heif_error_Ok) return 1;

  err = heif_image_add_plane(image, heif_channel_Y, 8, 8, 8);
  if (err.code != heif_error_Ok) return 2;

  err = heif_image_add_plane(image, heif_channel_Alpha, 9, 8, 8);
  if (err.code != heif_error_Ok) return 3;

  err = heif_image_extend_to_size_fill_with_zero(image, 8, 8);
  heif_image_release(image);
  return 0;
}
