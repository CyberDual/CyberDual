// Reproducer: an image that carries an extra data channel (heif_channel_filter_array)
// whose plane is stored at the chroma-subsampled resolution of a 4:2:0 image.
//
// HeifPixelImage::crop() bulk-copies every plane using coordinates derived from
// the image's declared size. It relies on has_standard_plane_sizes() to reject
// planes that do not cover that geometry. The filter_array plane below is only
// 64x64 while the image claims 128x128, so the row-wise memcpy in
// ComponentStorage::crop() runs past the end of the undersized plane.
//
// Expected: heif_image_crop() reports an error and no out-of-bounds access happens.

#include <stdio.h>

#include <libheif/heif.h>

int main(void)
{
  const int W = 128;
  const int H = 128;

  struct heif_image* img = NULL;
  struct heif_error err;

  err = heif_image_create(W, H, heif_colorspace_YCbCr, heif_chroma_420, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  // Standard planes, matching the declared 4:2:0 geometry.
  heif_image_add_plane(img, heif_channel_Y, W, H, 8);
  heif_image_add_plane(img, heif_channel_Cb, W / 2, H / 2, 8);
  heif_image_add_plane(img, heif_channel_Cr, W / 2, H / 2, 8);

  // Extra data channel. The caller stores it at the chroma resolution,
  // assuming the extra plane is subsampled like Cb/Cr.
  err = heif_image_add_plane(img, heif_channel_filter_array, W / 2, H / 2, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_add_plane(filter_array) failed: %s\n", err.message);
    heif_image_release(img);
    return 1;
  }

  // Full-area crop: every plane is copied over the declared image geometry.
  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_crop rejected the image as expected: %s\n", err.message);
    heif_image_release(img);
    return 0;
  }

  fprintf(stderr, "heif_image_crop unexpectedly succeeded\n");
  heif_image_release(img);
  return 2;
}
