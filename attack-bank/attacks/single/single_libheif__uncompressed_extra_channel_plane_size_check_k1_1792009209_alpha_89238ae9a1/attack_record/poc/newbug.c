#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(const char* what, struct heif_error err)
{
  fprintf(stderr, "%s: %s\n", what, err.message ? err.message : "(no message)");
  return 1;
}

int main(void)
{
  heif_init(NULL);

  const int W = 65;
  const int H = 65;
  heif_image* img = NULL;
  struct heif_error err = heif_image_create(W, H, heif_colorspace_YCbCr, heif_chroma_420, &img);
  if (err.code != heif_error_Ok) {
    return fail("heif_image_create", err);
  }

  err = heif_image_add_plane(img, heif_channel_Y, W, H, 8);
  if (err.code != heif_error_Ok) {
    return fail("add Y", err);
  }
  /* 4:2:0 chroma, round-up: (65 + 2 - 1) / 2 = 33 */
  err = heif_image_add_plane(img, heif_channel_Cb, 33, 33, 8);
  if (err.code != heif_error_Ok) {
    return fail("add Cb", err);
  }
  err = heif_image_add_plane(img, heif_channel_Cr, 33, 33, 8);
  if (err.code != heif_error_Ok) {
    return fail("add Cr", err);
  }
  err = heif_image_add_plane(img, heif_channel_depth, 64, 64, 8);
  if (err.code != heif_error_Ok) {
    return fail("add depth", err);
  }

  int stride = 0;
  uint8_t* y = heif_image_get_plane(img, heif_channel_Y, &stride);
  uint8_t* cb = heif_image_get_plane(img, heif_channel_Cb, &stride);
  uint8_t* cr = heif_image_get_plane(img, heif_channel_Cr, &stride);
  uint8_t* d = heif_image_get_plane(img, heif_channel_depth, &stride);
  if (!y || !cb || !cr || !d) {
    fprintf(stderr, "missing plane\n");
    return 1;
  }
  y[0] = 1;
  cb[0] = 2;
  cr[0] = 3;
  d[0] = 4;

  /* Full-image crop (zero margins). */
  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "crop rejected: %s\n", err.message ? err.message : "");
    heif_image_release(img);
    heif_deinit();
    return 0;
  }

  heif_image_release(img);
  heif_deinit();
  return 0;
}
