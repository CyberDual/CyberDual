// Decodes the primary image of a HEIF file to planar YCbCr 4:4:4 through the
// public libheif API, requesting an identity-matrix (GBR) NCLX output profile.
// Usage: decode_poc <input.heif>
#include <stdio.h>
#include "libheif/heif.h"

int main(int argc, char** argv)
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s input.heif\n", argv[0]);
    return 2;
  }

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_file(ctx, argv[1], NULL);
  if (err.code) {
    fprintf(stderr, "read: %s\n", err.message);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code) {
    fprintf(stderr, "handle: %s\n", err.message);
    return 1;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_profile_nclx* out_nclx = heif_nclx_color_profile_alloc();
  out_nclx->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
  out_nclx->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
  out_nclx->matrix_coefficients = heif_matrix_coefficients_RGB_GBR;
  out_nclx->full_range_flag = 1;
  options->output_image_nclx_profile = out_nclx;

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_444, options);
  if (err.code) {
    fprintf(stderr, "decode: %s\n", err.message);
    return 1;
  }

  int stride = 0;
  const uint8_t* y_plane = heif_image_get_plane_readonly(img, heif_channel_Y, &stride);
  printf("decoded ok: %dx%d, Y[0]=%d\n",
         heif_image_get_width(img, heif_channel_Y),
         heif_image_get_height(img, heif_channel_Y),
         y_plane ? y_plane[0] : -1);

  heif_image_release(img);
  heif_nclx_color_profile_free(out_nclx);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
