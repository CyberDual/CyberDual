#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Scale an odd-height monochrome image up by 2x. Both dimensions are
   positive and the plane matches the image, which is the contract of
   heif_image_create / heif_image_add_plane / heif_image_scale_image. */
int main(void)
{
  heif_image* img = NULL;
  struct heif_error err = heif_image_create(16, 9,
                                            heif_colorspace_monochrome,
                                            heif_chroma_monochrome,
                                            &img);
  if (err.code != heif_error_Ok) {
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, 16, 9, 8);
  if (err.code != heif_error_Ok) {
    return 2;
  }

  int stride = 0;
  uint8_t* src = heif_image_get_plane(img, heif_channel_Y, &stride);
  if (src == NULL || stride < 16) {
    return 3;
  }
  for (int y = 0; y < 9; y++) {
    memset(src + (size_t)y * (size_t)stride, (uint8_t)(0x10 + y), 16);
  }

  heif_image* scaled = NULL;
  err = heif_image_scale_image(img, &scaled, 16, 18, NULL);
  if (err.code != heif_error_Ok) {
    return 4;
  }

  int out_stride = 0;
  const uint8_t* dst = heif_image_get_plane_readonly(scaled, heif_channel_Y, &out_stride);
  if (dst == NULL) {
    return 5;
  }
  /* Keep the scaled pixels live so the call cannot be dropped. */
  volatile uint8_t sink = dst[0];
  (void)sink;

  heif_image_release(scaled);
  heif_image_release(img);
  return 0;
}
