#include <stdint.h>
#include <stdio.h>

#include "libheif/heif.h"
#include "libheif/heif_color.h"
#include "libheif/heif_context.h"
#include "libheif/heif_decoding.h"
#include "libheif/heif_encoding.h"
#include "libheif/heif_image.h"
#include "libheif/heif_image_handle.h"

static void report(const char* action, heif_error error)
{
  fprintf(stderr, "%s: %s\n", action,
          error.message ? error.message : "unknown error");
}

static heif_image* create_source_image(void)
{
  enum { width = 64, height = 64 };

  heif_image* image = NULL;
  heif_error error = heif_image_create(width, height, heif_colorspace_monochrome,
                                       heif_chroma_monochrome, &image);
  if (error.code != heif_error_Ok || image == NULL) {
    report("heif_image_create", error);
    return NULL;
  }

  error = heif_image_add_plane(image, heif_channel_Y, width, height, 16);
  if (error.code != heif_error_Ok) {
    report("heif_image_add_plane Y", error);
    heif_image_release(image);
    return NULL;
  }

  error = heif_image_add_plane(image, heif_channel_Alpha, width, height, 8);
  if (error.code != heif_error_Ok) {
    report("heif_image_add_plane alpha", error);
    heif_image_release(image);
    return NULL;
  }

  size_t stride = 0;
  uint8_t* y_plane = heif_image_get_plane2(image, heif_channel_Y, &stride);
  uint8_t* alpha_plane = heif_image_get_plane2(image, heif_channel_Alpha, &stride);
  if (y_plane == NULL || alpha_plane == NULL) {
    fprintf(stderr, "could not access image planes\n");
    heif_image_release(image);
    return NULL;
  }

  y_plane[0] = 128;
  y_plane[1] = 0;
  alpha_plane[0] = 128;
  return image;
}

static int run_case(const char* path, int use_overlay,
                    heif_colorspace target_colorspace, heif_chroma target_chroma,
                    heif_alpha_composition_mode composition_mode)
{
  heif_image* image = NULL;
  heif_context* context = NULL;
  heif_context* read_context = NULL;
  heif_encoder* encoder = NULL;
  heif_image_handle* encoded_handle = NULL;
  heif_image_handle* overlay_handle = NULL;
  heif_image_handle* primary_handle = NULL;
  heif_decoding_options* decode_options = NULL;
  heif_color_conversion_options_ext* conversion_options = NULL;
  heif_image* decoded = NULL;
  int result = 1;

  image = create_source_image();
  if (image == NULL) {
    goto cleanup;
  }

  context = heif_context_alloc();
  if (context == NULL) {
    fprintf(stderr, "heif_context_alloc failed\n");
    goto cleanup;
  }

  heif_error error = heif_context_get_encoder_for_format(
      context, heif_compression_uncompressed, &encoder);
  if (error.code != heif_error_Ok || encoder == NULL) {
    report("heif_context_get_encoder_for_format", error);
    goto cleanup;
  }

  error = heif_context_encode_image(context, image, encoder, NULL, &encoded_handle);
  if (error.code != heif_error_Ok || encoded_handle == NULL) {
    report("heif_context_encode_image", error);
    goto cleanup;
  }

  if (use_overlay) {
    heif_item_id encoded_id = heif_image_handle_get_item_id(encoded_handle);
    int32_t offsets[2] = {0, 0};
    uint16_t background[4] = {0xffff, 0xffff, 0xffff, 0xffff};
    error = heif_context_add_overlay_image(context, 64, 64, 1, &encoded_id,
                                           offsets, background, &overlay_handle);
    if (error.code != heif_error_Ok || overlay_handle == NULL) {
      report("heif_context_add_overlay_image", error);
      goto cleanup;
    }

    error = heif_context_set_primary_image(context, overlay_handle);
  }
  else {
    error = heif_context_set_primary_image(context, encoded_handle);
  }
  if (error.code != heif_error_Ok) {
    report("heif_context_set_primary_image", error);
    goto cleanup;
  }

  error = heif_context_write_to_file(context, path);
  if (error.code != heif_error_Ok) {
    report("heif_context_write_to_file", error);
    goto cleanup;
  }

  read_context = heif_context_alloc();
  if (read_context == NULL) {
    fprintf(stderr, "heif_context_alloc failed\n");
    goto cleanup;
  }

  error = heif_context_read_from_file(read_context, path, NULL);
  if (error.code != heif_error_Ok) {
    report("heif_context_read_from_file", error);
    goto cleanup;
  }

  error = heif_context_get_primary_image_handle(read_context, &primary_handle);
  if (error.code != heif_error_Ok || primary_handle == NULL) {
    report("heif_context_get_primary_image_handle", error);
    goto cleanup;
  }

  decode_options = heif_decoding_options_alloc();
  conversion_options = heif_color_conversion_options_ext_alloc();
  if (decode_options == NULL || conversion_options == NULL) {
    fprintf(stderr, "allocating decoding options failed\n");
    goto cleanup;
  }

  conversion_options->alpha_composition_mode = composition_mode;
  decode_options->color_conversion_options_ext = conversion_options;

  error = heif_decode_image(primary_handle, &decoded, target_colorspace,
                            target_chroma, decode_options);
  if (error.code != heif_error_Ok || decoded == NULL) {
    report("heif_decode_image", error);
    goto cleanup;
  }

  result = 0;

cleanup:
  if (decoded != NULL) heif_image_release(decoded);
  if (conversion_options != NULL) heif_color_conversion_options_ext_free(conversion_options);
  if (decode_options != NULL) heif_decoding_options_free(decode_options);
  if (primary_handle != NULL) heif_image_handle_release(primary_handle);
  if (overlay_handle != NULL) heif_image_handle_release(overlay_handle);
  if (encoded_handle != NULL) heif_image_handle_release(encoded_handle);
  if (encoder != NULL) heif_encoder_release(encoder);
  if (read_context != NULL) heif_context_free(read_context);
  if (context != NULL) heif_context_free(context);
  if (image != NULL) heif_image_release(image);
  return result;
}

int main(void)
{
  run_case("/tmp/libheif-benchmark-overlay.heif", 1,
           heif_colorspace_RGB, heif_chroma_444,
           heif_alpha_composition_mode_solid_color);
  run_case("/tmp/libheif-benchmark-direct.heif", 0,
           heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_LE,
           heif_alpha_composition_mode_none);
  return 0;
}
