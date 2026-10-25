#include <libheif/heif.h>

int main(void)
{
  struct heif_image* image = NULL;
  if (heif_image_create(2, 128, heif_colorspace_monochrome,
                        heif_chroma_monochrome, &image).code != heif_error_Ok) {
    return 1;
  }
  if (heif_image_add_plane(image, heif_channel_Y, 2, 128, 8).code != heif_error_Ok ||
      heif_image_add_plane(image, heif_channel_Alpha, 2, 1, 8).code != heif_error_Ok) {
    heif_image_release(image);
    return 2;
  }
  heif_error err = heif_image_extend_to_size_fill_with_zero(image, 3, 129);
  heif_image_release(image);
  return err.code == heif_error_Ok || err.code == heif_error_Unsupported_feature ? 0 : 3;
}
