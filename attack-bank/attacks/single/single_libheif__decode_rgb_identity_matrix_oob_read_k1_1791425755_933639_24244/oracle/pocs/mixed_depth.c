#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void check(heif_error err, const char* what)
{
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "%s: %s\n", what, err.message);
    exit(2);
  }
}

int main(void)
{
  const int width = 64;
  const int height = 64;
  heif_image* image = NULL;
  heif_context* encoder_context = NULL;
  heif_context* decoder_context = NULL;
  heif_encoder* encoder = NULL;
  heif_image_handle* handle = NULL;
  heif_image* decoded = NULL;
  char path[] = "/tmp/libheif-mixed-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return 2;
  close(fd);

  check(heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_444, &image), "image");
  check(heif_image_add_plane(image, heif_channel_R, width, height, 8), "R");
  check(heif_image_add_plane(image, heif_channel_G, width, height, 16), "G");
  check(heif_image_add_plane(image, heif_channel_B, width, height, 8), "B");
  for (int channel = heif_channel_R; channel <= heif_channel_B; channel++) {
    int stride;
    uint8_t* plane = heif_image_get_plane(image, (heif_channel) channel, &stride);
    memset(plane, 0x40, (size_t) stride * height);
  }
  heif_color_profile_nclx* profile = heif_nclx_color_profile_alloc();
  check(heif_nclx_color_profile_set_matrix_coefficients(profile, heif_matrix_coefficients_RGB_GBR), "matrix");
  profile->full_range_flag = 1;
  check(heif_image_set_nclx_color_profile(image, profile), "profile");
  heif_nclx_color_profile_free(profile);

  encoder_context = heif_context_alloc();
  check(heif_context_get_encoder_for_format(encoder_context, heif_compression_uncompressed, &encoder), "encoder");
  check(heif_context_encode_image(encoder_context, image, encoder, NULL, NULL), "encode");
  check(heif_context_write_to_file(encoder_context, path), "write");
  decoder_context = heif_context_alloc();
  check(heif_context_read_from_file(decoder_context, path, NULL), "read");
  unlink(path);
  check(heif_context_get_primary_image_handle(decoder_context, &handle), "handle");
  check(heif_decode_image(handle, &decoded, heif_colorspace_YCbCr, heif_chroma_444, NULL), "decode");
  heif_image_release(decoded);
  heif_image_handle_release(handle);
  heif_context_free(decoder_context);
  heif_encoder_release(encoder);
  heif_context_free(encoder_context);
  heif_image_release(image);
  return 0;
}
