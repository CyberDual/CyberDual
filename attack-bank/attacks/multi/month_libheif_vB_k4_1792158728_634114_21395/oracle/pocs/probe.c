#include <libheif/heif.h>
#include <string.h>

/* Scale a 32-bit monochrome image. The sample width is in contract for
   heif_image_add_plane(); nearest-neighbor scaling is part of the public API. */
int main(void)
{
  heif_image* image = NULL;
  heif_image* scaled = NULL;
  heif_error err;
  int size = 128;

  err = heif_image_create(size, size, heif_colorspace_monochrome, heif_chroma_monochrome, &image);
  if (err.code != heif_error_Ok) {
    return 0;
  }

  err = heif_image_add_plane(image, heif_channel_Y, size, size, 32);
  if (err.code != heif_error_Ok) {
    heif_image_release(image);
    return 0;
  }

  int stride = 0;
  uint8_t* plane = heif_image_get_plane(image, heif_channel_Y, &stride);
  if (plane == NULL || stride < size * 4) {
    heif_image_release(image);
    return 0;
  }
  memset(plane, 0x5a, (size_t)stride * (size_t)size);

  err = heif_image_scale_image(image, &scaled, size, size, NULL);
  if (scaled != NULL) {
    heif_image_release(scaled);
  }
  heif_image_release(image);
  (void)err;
  return 0;
}
