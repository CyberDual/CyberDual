#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>

/* Monochrome image with an auxiliary depth plane, cropped through the
 * public heif_image_crop() entry point.
 */
int main(void)
{
  heif_image* img = NULL;
  struct heif_error err;

  err = heif_image_create(128, 128, heif_colorspace_monochrome, heif_chroma_monochrome, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "create: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, 128, 128, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Y: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_depth, 128, 8, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add depth: %s\n", err.message);
    return 1;
  }

  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "crop: %s\n", err.message);
    heif_image_release(img);
    return 0;
  }

  heif_image_release(img);
  return 0;
}
