#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libheif/heif.h"

static void fill_plane(struct heif_image* image, enum heif_channel ch, int w, int h, uint8_t value)
{
  int stride = 0;
  uint8_t* p = heif_image_get_plane(image, ch, &stride);
  for (int y = 0; y < h; y++) {
    memset(p + y * stride, value, (size_t) w);
  }
}

int main(void)
{
  const int W = 200;
  const int H = 200;

  struct heif_image* image = NULL;
  struct heif_error err = heif_image_create(W, H, heif_colorspace_RGB, heif_chroma_444, &image);
  if (err.code != heif_error_Ok) { fprintf(stderr, "create failed\n"); return 1; }

  heif_image_add_plane(image, heif_channel_R, W, H, 8);
  heif_image_add_plane(image, heif_channel_G, W, H, 8);
  heif_image_add_plane(image, heif_channel_B, W, H, 8);

  fill_plane(image, heif_channel_R, W, H, 10);
  fill_plane(image, heif_channel_G, W, H, 20);
  fill_plane(image, heif_channel_B, W, H, 30);

  struct heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();
  nclx->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
  nclx->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_709_5;
  heif_nclx_color_profile_set_matrix_coefficients(nclx, 0 /* identity / GBR */);
  nclx->full_range_flag = 1;
  heif_image_set_nclx_color_profile(image, nclx);

  struct heif_context* enc_ctx = heif_context_alloc();
  struct heif_encoder* encoder = NULL;
  err = heif_context_get_encoder_for_format(enc_ctx, heif_compression_uncompressed, &encoder);
  if (err.code != heif_error_Ok) { fprintf(stderr, "no uncompressed encoder\n"); return 1; }

  struct heif_encoding_options* enc_options = heif_encoding_options_alloc();
  err = heif_context_encode_image(enc_ctx, image, encoder, enc_options, NULL);
  if (err.code != heif_error_Ok) { fprintf(stderr, "encode failed: %s\n", err.message); return 1; }
  heif_encoding_options_free(enc_options);
  heif_encoder_release(encoder);

  const char* path = "/tmp/newbug_poc.heif";
  err = heif_context_write_to_file(enc_ctx, path);
  if (err.code != heif_error_Ok) { fprintf(stderr, "write failed\n"); return 1; }

  heif_image_release(image);
  heif_context_free(enc_ctx);
  heif_nclx_color_profile_free(nclx);

  struct heif_context* dec_ctx = heif_context_alloc();
  err = heif_context_read_from_file(dec_ctx, path, NULL);
  if (err.code != heif_error_Ok) { fprintf(stderr, "read failed\n"); return 1; }

  struct heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(dec_ctx, &handle);
  if (err.code != heif_error_Ok) { fprintf(stderr, "no handle\n"); return 1; }

  struct heif_decoding_options* dec_options = heif_decoding_options_alloc();
  dec_options->output_image_nclx_profile_passthrough = 1;
  dec_options->color_conversion_options.preferred_chroma_downsampling_algorithm = heif_chroma_downsampling_nearest_neighbor;
  dec_options->color_conversion_options.only_use_preferred_chroma_algorithm = 1;

  struct heif_image* decoded = NULL;
  err = heif_decode_image(handle, &decoded, heif_colorspace_YCbCr, heif_chroma_420, dec_options);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode failed (rc visible, no crash): %s\n", err.message);
    return 1;
  }

  printf("decoded ok (no crash)\n");

  heif_decoding_options_free(dec_options);
  heif_image_release(decoded);
  heif_image_handle_release(handle);
  heif_context_free(dec_ctx);

  return 0;
}
