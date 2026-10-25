#include "libheif/heif.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void fail(const heif_error& error, const char* operation)
{
  std::fprintf(stderr, "%s: %s\n", operation, error.message ? error.message : "unknown error");
  std::exit(1);
}

void check(const heif_error& error, const char* operation)
{
  if (error.code != heif_error_Ok) {
    fail(error, operation);
  }
}

void fill_plane(heif_image* image, heif_channel channel, uint8_t value)
{
  int stride = 0;
  uint8_t* plane = heif_image_get_plane(image, channel, &stride);
  if (!plane) {
    std::fprintf(stderr, "failed to get image plane\n");
    std::exit(1);
  }

  int width = heif_image_get_width(image, channel);
  int height = heif_image_get_height(image, channel);
  int bytes_per_pixel = heif_image_get_bits_per_pixel(image, channel) / 8;
  for (int y = 0; y < height; ++y) {
    std::memset(plane + y * static_cast<size_t>(stride), value,
                static_cast<size_t>(width) * bytes_per_pixel);
  }
}

}

int main(int argc, char** argv)
{
  const char* output_path = argc > 1 ? argv[1] : "residual.heif";

  heif_image* image = nullptr;
  check(heif_image_create(64, 64, heif_colorspace_RGB, heif_chroma_planar, &image),
        "heif_image_create");

  check(heif_image_add_plane(image, heif_channel_R, 64, 64, 8),
        "add R plane");
  check(heif_image_add_plane(image, heif_channel_G, 64, 64, 8),
        "add G plane");
  check(heif_image_add_plane(image, heif_channel_B, 64, 64, 16),
        "add B plane");

  fill_plane(image, heif_channel_R, 0x11);
  fill_plane(image, heif_channel_G, 0x22);
  fill_plane(image, heif_channel_B, 0x33);

  heif_color_profile_nclx* profile = heif_nclx_color_profile_alloc();
  if (!profile) {
    std::fprintf(stderr, "failed to allocate color profile\n");
    return 1;
  }
  check(heif_nclx_color_profile_set_color_primaries(profile, 1),
        "set color primaries");
  check(heif_nclx_color_profile_set_transfer_characteristics(profile, 13),
        "set transfer characteristics");
  check(heif_nclx_color_profile_set_matrix_coefficients(profile, 0),
        "set matrix coefficients");
  profile->full_range_flag = 0;
  check(heif_image_set_nclx_color_profile(image, profile),
        "set color profile");
  heif_nclx_color_profile_free(profile);

  heif_context* context = heif_context_alloc();
  heif_encoder* encoder = nullptr;
  check(heif_context_get_encoder_for_format(context, heif_compression_uncompressed, &encoder),
        "get encoder");
  heif_encoding_options* encoding_options = heif_encoding_options_alloc();
  heif_image_handle* handle = nullptr;
  check(heif_context_encode_image(context, image, encoder, encoding_options, &handle),
        "encode image");
  heif_encoder_release(encoder);
  heif_encoding_options_free(encoding_options);
  heif_image_handle_release(handle);
  check(heif_context_write_to_file(context, output_path),
        "write file");

  heif_image_release(image);
  heif_context_free(context);

  context = heif_context_alloc();
  check(heif_context_read_from_file(context, output_path, nullptr),
        "read file");
  check(heif_context_get_primary_image_handle(context, &handle),
        "get image handle");
  image = nullptr;
  heif_decoding_options* decoding_options = heif_decoding_options_alloc();
  decoding_options->output_image_nclx_profile_passthrough = 1;
  heif_error decode_error = heif_decode_image(handle, &image, heif_colorspace_YCbCr,
                                              heif_chroma_444, decoding_options);
  heif_decoding_options_free(decoding_options);
  if (decode_error.code == heif_error_Unsupported_feature &&
      decode_error.subcode == heif_suberror_Unspecified) {
    heif_image_handle_release(handle);
    heif_context_free(context);
    return 0;
  }
  check(decode_error, "decode image");

  heif_image_release(image);
  heif_image_handle_release(handle);
  heif_context_free(context);
  return 0;
}
