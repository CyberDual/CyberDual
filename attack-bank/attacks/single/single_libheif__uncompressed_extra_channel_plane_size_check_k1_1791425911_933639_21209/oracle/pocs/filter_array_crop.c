#include <libheif/heif.h>
#include <libheif/heif_uncompressed.h>
#include <stdio.h>

int main(void)
{
  const char* path = "/tmp/filter_array_crop.heif";
  heif_image* source = NULL;
  heif_error err = heif_image_create(32, 65, heif_colorspace_filter_array,
                                     heif_chroma_planar, &source);
  if (err.code != heif_error_Ok) return 1;
  err = heif_image_add_plane(source, heif_channel_filter_array, 32, 65, 8);
  if (err.code != heif_error_Ok) return 2;

  heif_context* writer = heif_context_alloc();
  heif_encoder* encoder = NULL;
  err = heif_context_get_encoder_for_format(writer, heif_compression_uncompressed, &encoder);
  if (err.code != heif_error_Ok) return 3;
  err = heif_context_encode_image(writer, source, encoder, NULL, NULL);
  if (err.code != heif_error_Ok) return 4;
  err = heif_context_write_to_file(writer, path);
  if (err.code != heif_error_Ok) return 5;
  heif_encoder_release(encoder);
  heif_image_release(source);
  heif_context_free(writer);

  heif_context* reader = heif_context_alloc();
  err = heif_context_read_from_file(reader, path, NULL);
  if (err.code != heif_error_Ok) return 6;
  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(reader, &handle);
  if (err.code != heif_error_Ok) return 7;
  heif_image* decoded = NULL;
  err = heif_decode_image(handle, &decoded, heif_colorspace_undefined,
                          heif_chroma_undefined, NULL);
  if (err.code != heif_error_Ok) return 8;

  err = heif_image_add_plane(decoded, heif_channel_depth, 32, 64, 8);
  if (err.code != heif_error_Ok) return 9;
  err = heif_image_crop(decoded, 0, 0, 0, 0);

  heif_image_release(decoded);
  heif_image_handle_release(handle);
  heif_context_free(reader);
  remove(path);
  return err.code == heif_error_Ok ? 0 : 10;
}
