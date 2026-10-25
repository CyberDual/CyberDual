#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct buffer {
  unsigned char data[1 << 20];
  size_t size;
};

static struct heif_error write_buffer(struct heif_context* ctx, const void* data,
                                      size_t size, void* userdata)
{
  struct buffer* out = (struct buffer*)userdata;
  (void)ctx;
  if (size > sizeof(out->data) - out->size) {
    return (struct heif_error){heif_error_Memory_allocation_error, heif_suberror_Unspecified, "buffer full"};
  }
  memcpy(out->data + out->size, data, size);
  out->size += size;
  return (struct heif_error){heif_error_Ok, heif_suberror_Unspecified, "Success"};
}

static int check(struct heif_error err, const char* step)
{
  if (err.code == heif_error_Ok) return 1;
  fprintf(stderr, "%s: %s\n", step, err.message);
  return 0;
}

int main(void)
{
  enum { WIDTH = 64, HEIGHT = 64 };
  struct buffer bytes = {{0}, 0};
  struct heif_image* image = NULL;
  struct heif_context* output = NULL;
  struct heif_context* input = NULL;
  struct heif_encoder* encoder = NULL;
  struct heif_image_handle* handle = NULL;
  struct heif_decoding_options* options = NULL;
  struct heif_color_conversion_options_ext* ext = NULL;
  struct heif_image* decoded = NULL;
  struct heif_writer writer = {1, write_buffer};
  size_t stride;
  uint8_t* plane;
  int rc = 1;

  if (!check(heif_image_create(WIDTH, HEIGHT, heif_colorspace_monochrome,
                               heif_chroma_monochrome, &image), "create")) goto done;
  if (!check(heif_image_add_plane(image, heif_channel_Y, WIDTH, HEIGHT, 16), "Y plane")) goto done;
  if (!check(heif_image_add_plane(image, heif_channel_Alpha, WIDTH, HEIGHT, 8), "alpha plane")) goto done;
  plane = heif_image_get_plane2(image, heif_channel_Y, &stride);
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      uint16_t value = 0x8000;
      memcpy(plane + y * stride + 2 * x, &value, sizeof(value));
    }
  }
  plane = heif_image_get_plane2(image, heif_channel_Alpha, &stride);
  for (int y = 0; y < HEIGHT; ++y) memset(plane + y * stride, 0x80, WIDTH);

  output = heif_context_alloc();
  if (!check(heif_context_get_encoder_for_format(output, heif_compression_uncompressed,
                                                  &encoder), "encoder")) goto done;
  if (!check(heif_context_encode_image(output, image, encoder, NULL, NULL), "encode")) goto done;
  if (!check(heif_context_write(output, &writer, &bytes), "write")) goto done;

  input = heif_context_alloc();
  if (!check(heif_context_read_from_memory_without_copy(input, bytes.data, bytes.size, NULL),
             "read")) goto done;
  if (!check(heif_context_get_primary_image_handle(input, &handle), "handle")) goto done;
  options = heif_decoding_options_alloc();
  ext = heif_color_conversion_options_ext_alloc();
  if (!options || !ext) goto done;
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = ext->background_green = ext->background_blue = 0xffff;
  options->color_conversion_options_ext = ext;
  if (!check(heif_decode_image(handle, &decoded, heif_colorspace_monochrome,
                                heif_chroma_monochrome, options), "decode")) goto done;
  rc = 0;

done:
  if (decoded) heif_image_release(decoded);
  if (ext) heif_color_conversion_options_ext_free(ext);
  if (options) heif_decoding_options_free(options);
  if (handle) heif_image_handle_release(handle);
  if (input) heif_context_free(input);
  if (encoder) heif_encoder_release(encoder);
  if (output) heif_context_free(output);
  if (image) heif_image_release(image);
  return rc;
}
