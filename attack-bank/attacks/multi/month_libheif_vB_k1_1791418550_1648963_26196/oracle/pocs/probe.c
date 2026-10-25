#include "libheif/heif.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct output_buffer { uint8_t *data; size_t size; size_t capacity; };

static heif_error write_bytes(heif_context *ctx, const void *data, size_t size, void *userdata)
{
  struct output_buffer *out = userdata;
  heif_error ok = {heif_error_Ok, heif_suberror_Unspecified, "Success"};
  heif_error no_memory = {heif_error_Memory_allocation_error, heif_suberror_Unspecified, "allocation failed"};
  (void)ctx;
  if (size > SIZE_MAX - out->size) return no_memory;
  if (out->size + size > out->capacity) {
    size_t capacity = out->size + size;
    uint8_t *data_new = realloc(out->data, capacity);
    if (!data_new) return no_memory;
    out->data = data_new;
    out->capacity = capacity;
  }
  if (size) memcpy(out->data + out->size, data, size);
  out->size += size;
  return ok;
}

int main(void)
{
  const int width = 256;
  const int height = 64;
  heif_image *source = NULL, *decoded = NULL;
  heif_context *ctx = NULL;
  heif_encoder *encoder = NULL;
  heif_image_handle *handle = NULL;
  heif_context *read_ctx = NULL;
  struct output_buffer output = {0};
  heif_writer writer = {1, write_bytes};
  heif_decoding_options *options = NULL;
  heif_color_conversion_options_ext *composition = NULL;
  heif_error err = {heif_error_Ok, heif_suberror_Unspecified, "Success"};
  int stride;
  uint8_t *plane;

  err = heif_image_create(width, height, heif_colorspace_monochrome,
                          heif_chroma_monochrome, &source);
  if (err.code) goto fail;
  err = heif_image_add_plane(source, heif_channel_Y, width, height, 16);
  if (err.code) goto fail;
  err = heif_image_add_plane(source, heif_channel_Alpha, width, height, 8);
  if (err.code) goto fail;
  plane = heif_image_get_plane(source, heif_channel_Y, &stride);
  if (!plane) goto fail;
  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      uint16_t sample = 0x8000;
      memcpy(plane + y * stride + 2 * x, &sample, sizeof(sample));
    }
  plane = heif_image_get_plane(source, heif_channel_Alpha, &stride);
  if (!plane) goto fail;
  for (int y = 0; y < height; y++)
    memset(plane + y * stride, 0x80, width);

  ctx = heif_context_alloc();
  if (!ctx) goto fail;
  err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &encoder);
  if (err.code) goto fail;
  err = heif_context_encode_image(ctx, source, encoder, NULL, NULL);
  if (err.code) goto fail;
  err = heif_context_write(ctx, &writer, &output);
  if (err.code) goto fail;
  read_ctx = heif_context_alloc();
  if (!read_ctx) goto fail;
  err = heif_context_read_from_memory_without_copy(read_ctx, output.data, output.size, NULL);
  if (err.code) goto fail;
  err = heif_context_get_primary_image_handle(read_ctx, &handle);
  if (err.code) goto fail;
  options = heif_decoding_options_alloc();
  composition = heif_color_conversion_options_ext_alloc();
  if (!options || !composition) goto fail;
  composition->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  composition->background_red = 0xffff;
  composition->background_green = 0xffff;
  composition->background_blue = 0xffff;
  options->color_conversion_options_ext = composition;
  err = heif_decode_image(handle, &decoded, heif_colorspace_monochrome,
                          heif_chroma_monochrome, options);
  if (err.code != heif_error_Ok && err.code != heif_error_Unsupported_feature) goto fail;

  if (decoded) heif_image_release(decoded);
  heif_color_conversion_options_ext_free(composition);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_encoder_release(encoder);
  heif_context_free(ctx);
  heif_context_free(read_ctx);
  heif_image_release(source);
  free(output.data);
  return 0;

fail:
  fprintf(stderr, "probe API error: %d (%s)\n", err.code, err.message ? err.message : "unknown");
  if (decoded) heif_image_release(decoded);
  if (composition) heif_color_conversion_options_ext_free(composition);
  if (options) heif_decoding_options_free(options);
  if (handle) heif_image_handle_release(handle);
  if (encoder) heif_encoder_release(encoder);
  if (ctx) heif_context_free(ctx);
  if (read_ctx) heif_context_free(read_ctx);
  if (source) heif_image_release(source);
  free(output.data);
  return 2;
}
