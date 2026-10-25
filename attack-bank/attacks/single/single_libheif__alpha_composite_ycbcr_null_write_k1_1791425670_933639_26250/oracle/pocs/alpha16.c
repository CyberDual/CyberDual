#include <libheif/heif.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void check(heif_error err)
{
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "%s\n", err.message);
    exit(1);
  }
}

int main(int argc, char** argv)
{
  const int width = 96;
  const int height = 96;
  const int depth = argc > 1 && argv[1][0] == '8' ? 8 : 16;
  heif_image* image = NULL;
  check(heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_444, &image));
  heif_channel channels[] = {heif_channel_R, heif_channel_G, heif_channel_B, heif_channel_Alpha};
  for (int i = 0; i < 4; ++i) {
    check(heif_image_add_plane(image, channels[i], width, height, depth));
    size_t stride = 0;
    uint8_t* plane = heif_image_get_plane2(image, channels[i], &stride);
    if (!plane) return 2;
    for (int y = 0; y < height; ++y) {
      if (depth == 8) {
        uint8_t* row = plane + y * stride;
        for (int x = 0; x < width; ++x) row[x] = i == 3 ? 0x80 : 0x60;
      } else {
        uint16_t* row = (uint16_t*)(plane + y * stride);
        for (int x = 0; x < width; ++x) row[x] = i == 3 ? 0x8000 : 0x6000;
      }
    }
  }

  heif_context* context = heif_context_alloc();
  heif_encoder* encoder = NULL;
  check(heif_context_get_encoder_for_format(context, heif_compression_uncompressed, &encoder));
  heif_encoding_options* encoding = heif_encoding_options_alloc();
  encoding->macOS_compatibility_workaround_no_nclx_profile = 1;
  heif_image_handle* handle = NULL;
  check(heif_context_encode_image(context, image, encoder, encoding, &handle));

  heif_decoding_options* decoding = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xffff;
  ext->background_green = 0xffff;
  ext->background_blue = 0xffff;
  decoding->color_conversion_options_ext = ext;
  heif_image* output = NULL;
  check(heif_decode_image(handle, &output,
                          depth == 8 ? heif_colorspace_YCbCr : heif_colorspace_RGB,
                          depth == 8 ? heif_chroma_420 : heif_chroma_interleaved_RRGGBB_LE,
                          decoding));
  heif_image_release(output);
  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(decoding);
  heif_image_handle_release(handle);
  heif_encoding_options_free(encoding);
  heif_encoder_release(encoder);
  heif_context_free(context);
  heif_image_release(image);
  return 0;
}
