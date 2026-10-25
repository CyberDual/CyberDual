#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "libheif/heif.h"


static int fill_plane(heif_image* image, heif_channel channel, uint16_t value)
{
  size_t stride = 0;
  uint8_t* plane = heif_image_get_plane2(image, channel, &stride);
  if (plane == NULL) {
    return -1;
  }

  uint16_t* samples = (uint16_t*) plane;
  stride /= sizeof(uint16_t);
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
      samples[y * stride + x] = value;
    }
  }

  return 0;
}


int main(void)
{
  heif_image* image = NULL;
  heif_context* context = NULL;
  heif_encoder* encoder = NULL;
  heif_encoding_options* encoding_options = NULL;
  heif_image_handle* handle = NULL;
  heif_decoding_options* decoding_options = NULL;
  heif_color_conversion_options_ext* conversion_options = NULL;
  heif_image* decoded = NULL;
  int result = 1;

  heif_error err = heif_image_create(64, 64, heif_colorspace_RGB,
                                     heif_chroma_444, &image);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "create: %s\n", err.message);
    goto cleanup;
  }

  for (int i = 0; i < 4; i++) {
    static const heif_channel channels[4] = {
        heif_channel_R, heif_channel_G, heif_channel_B, heif_channel_Alpha
    };
    err = heif_image_add_plane(image, channels[i], 64, 64, 9);
    if (err.code != heif_error_Ok) {
      fprintf(stderr, "add plane: %s\n", err.message);
      goto cleanup;
    }
    if (fill_plane(image, channels[i], 0x1ff) != 0) {
      fprintf(stderr, "fill plane failed\n");
      goto cleanup;
    }
  }

  context = heif_context_alloc();
  if (context == NULL) {
    goto cleanup;
  }

  err = heif_context_get_encoder_for_format(context,
                                             heif_compression_uncompressed,
                                             &encoder);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "encoder: %s\n", err.message);
    goto cleanup;
  }

  encoding_options = heif_encoding_options_alloc();
  if (encoding_options == NULL) {
    goto cleanup;
  }

  err = heif_context_encode_image(context, image, encoder,
                                   encoding_options, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "encode: %s\n", err.message);
    goto cleanup;
  }

  decoding_options = heif_decoding_options_alloc();
  conversion_options = heif_color_conversion_options_ext_alloc();
  if (decoding_options == NULL || conversion_options == NULL) {
    goto cleanup;
  }

  conversion_options->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  conversion_options->background_red = 0xffff;
  conversion_options->background_green = 0xffff;
  conversion_options->background_blue = 0xffff;
  decoding_options->color_conversion_options_ext = conversion_options;

  err = heif_decode_image(handle, &decoded, heif_colorspace_RGB,
                          heif_chroma_444, decoding_options);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode: %s\n", err.message);
    goto cleanup;
  }

  result = 0;

cleanup:
  if (decoded != NULL) {
    heif_image_release(decoded);
  }
  if (conversion_options != NULL) {
    heif_color_conversion_options_ext_free(conversion_options);
  }
  if (decoding_options != NULL) {
    heif_decoding_options_free(decoding_options);
  }
  if (handle != NULL) {
    heif_image_handle_release(handle);
  }
  if (encoding_options != NULL) {
    heif_encoding_options_free(encoding_options);
  }
  if (encoder != NULL) {
    heif_encoder_release(encoder);
  }
  if (context != NULL) {
    heif_context_free(context);
  }
  if (image != NULL) {
    heif_image_release(image);
  }

  return result;
}
