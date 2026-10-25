#include <stdio.h>
#include <stdlib.h>
#include <libheif/heif.h>

static void die(const char* what, struct heif_error err)
{
  fprintf(stderr, "%s: %s\n", what, err.message ? err.message : "(no message)");
  exit(2);
}

int main(void)
{
  const int w = 80;
  const int h = 80;

  struct heif_error err = heif_init(NULL);
  if (err.code) die("heif_init", err);

  struct heif_image* image = NULL;
  err = heif_image_create(w, h, heif_colorspace_RGB, heif_chroma_444, &image);
  if (err.code) die("heif_image_create", err);

  err = heif_image_add_plane(image, heif_channel_R, w, h, 8);
  if (err.code) die("add R", err);
  err = heif_image_add_plane(image, heif_channel_G, w, h, 8);
  if (err.code) die("add G", err);
  err = heif_image_add_plane(image, heif_channel_B, w, h, 16);
  if (err.code) die("add B", err);

  int r_stride = 0, g_stride = 0, b_stride = 0;
  uint8_t* r = heif_image_get_plane(image, heif_channel_R, &r_stride);
  uint8_t* g = heif_image_get_plane(image, heif_channel_G, &g_stride);
  uint8_t* b = heif_image_get_plane(image, heif_channel_B, &b_stride);
  if (!r || !g || !b) {
    fprintf(stderr, "missing plane\n");
    return 2;
  }
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      r[y * r_stride + x] = (uint8_t)(x + y);
      g[y * g_stride + x] = (uint8_t)(2 * x + y);
      b[y * b_stride + 2 * x] = (uint8_t)x;
      b[y * b_stride + 2 * x + 1] = (uint8_t)y;
    }
  }

  /* Identity matrix, limited range. ISO/IEC 23091 stores these as independent
     nclx fields; libheif accepts the combination and converts it. */
  struct heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();
  err = heif_nclx_color_profile_set_matrix_coefficients(nclx, heif_matrix_coefficients_RGB_GBR);
  if (err.code) die("set matrix", err);
  nclx->full_range_flag = 0;
  err = heif_image_set_nclx_color_profile(image, nclx);
  if (err.code) die("set image nclx", err);

  struct heif_context* ctx = heif_context_alloc();
  struct heif_encoder* encoder = NULL;
  err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &encoder);
  if (err.code) die("get encoder", err);

  struct heif_encoding_options* enc = heif_encoding_options_alloc();
  enc->output_nclx_profile = nclx;
  err = heif_context_encode_image(ctx, image, encoder, enc, NULL);
  if (err.code) die("encode", err);

  const char* path = "/tmp/mixed_rgb.heif";
  err = heif_context_write_to_file(ctx, path);
  if (err.code) die("write", err);

  heif_encoding_options_free(enc);
  heif_encoder_release(encoder);
  heif_image_release(image);
  heif_context_free(ctx);
  heif_nclx_color_profile_free(nclx);

  struct heif_context* ctx2 = heif_context_alloc();
  err = heif_context_read_from_file(ctx2, path, NULL);
  if (err.code) die("read", err);

  struct heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx2, &handle);
  if (err.code) die("handle", err);

  struct heif_decoding_options* dec = heif_decoding_options_alloc();
  dec->output_image_nclx_profile_passthrough = 1;

  struct heif_image* out = NULL;
  err = heif_decode_image(handle, &out, heif_colorspace_YCbCr, heif_chroma_444, dec);
  if (err.code) {
    fprintf(stderr, "decode returned error: %s\n", err.message ? err.message : "");
    heif_decoding_options_free(dec);
    heif_image_handle_release(handle);
    heif_context_free(ctx2);
    heif_deinit();
    return 1;
  }

  int stride = 0;
  const uint8_t* yplane = heif_image_get_plane_readonly(out, heif_channel_Y, &stride);
  if (yplane && stride > 0) {
    fprintf(stderr, "decoded Y[0]=%u\n", yplane[0]);
  }

  heif_image_release(out);
  heif_decoding_options_free(dec);
  heif_image_handle_release(handle);
  heif_context_free(ctx2);
  heif_deinit();
  return 0;
}
