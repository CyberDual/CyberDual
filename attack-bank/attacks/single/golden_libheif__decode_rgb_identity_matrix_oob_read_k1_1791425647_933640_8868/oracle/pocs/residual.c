#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check(struct heif_error err, const char* step)
{
  if (err.code == heif_error_Ok) {
    return 1;
  }
  fprintf(stderr, "%s: %s\n", step, err.message);
  return 0;
}

int main(void)
{
  const int width = 512, height = 512;
  struct heif_image* image = NULL;
  struct heif_context* encoded = NULL;
  struct heif_context* decoded = NULL;
  struct heif_encoder* encoder = NULL;
  struct heif_image_handle* handle = NULL;
  struct heif_image* output = NULL;
  struct heif_color_profile_nclx* profile = NULL;
  const char* path = "/tmp/libheif-mixed-rgb.heif";

  if (!check(heif_image_create(width, height, heif_colorspace_RGB,
                               heif_chroma_444, &image), "create image")) return 1;
  if (!check(heif_image_add_plane(image, heif_channel_R, width, height, 16), "R plane")) return 1;
  if (!check(heif_image_add_plane(image, heif_channel_G, width, height, 16), "G plane")) return 1;
  if (!check(heif_image_add_plane(image, heif_channel_B, width, height, 8), "B plane")) return 1;

  int stride;
  uint8_t* plane = heif_image_get_plane(image, heif_channel_R, &stride);
  memset(plane, 0, (size_t)stride * height);
  plane = heif_image_get_plane(image, heif_channel_G, &stride);
  memset(plane, 0, (size_t)stride * height);
  plane = heif_image_get_plane(image, heif_channel_B, &stride);
  memset(plane, 0, (size_t)stride * height);

  profile = heif_nclx_color_profile_alloc();
  if (!profile) return 1;
  if (!check(heif_nclx_color_profile_set_matrix_coefficients(profile, 0), "matrix")) return 1;
  profile->full_range_flag = 1;
  if (!check(heif_image_set_nclx_color_profile(image, profile), "profile")) return 1;
  heif_nclx_color_profile_free(profile);

  encoded = heif_context_alloc();
  if (!check(heif_context_get_encoder_for_format(encoded, heif_compression_uncompressed,
                                                 &encoder), "encoder")) return 1;
  if (!check(heif_context_encode_image(encoded, image, encoder, NULL, NULL), "encode")) return 1;
  if (!check(heif_context_write_to_file(encoded, path), "write")) return 1;
  heif_encoder_release(encoder);
  heif_image_release(image);
  heif_context_free(encoded);

  decoded = heif_context_alloc();
  if (!check(heif_context_read_from_file(decoded, path, NULL), "read")) return 1;
  if (!check(heif_context_get_primary_image_handle(decoded, &handle), "handle")) return 1;
  struct heif_error err = heif_decode_image(handle, &output, heif_colorspace_YCbCr,
                                            heif_chroma_444, NULL);
  if (err.code == heif_error_Ok) {
    heif_image_release(output);
  } else {
    fprintf(stderr, "decode: %s\n", err.message);
  }
  heif_image_handle_release(handle);
  heif_context_free(decoded);
  remove(path);
  return err.code == heif_error_Ok ? 0 : 2;
}
