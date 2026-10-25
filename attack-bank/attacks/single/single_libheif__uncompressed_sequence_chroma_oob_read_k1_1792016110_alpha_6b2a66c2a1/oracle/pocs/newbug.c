#include <libheif/heif.h>
#include <libheif/heif_components.h>
#include <libheif/heif_sequences.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(heif_error error, const char* action)
{
  if (error.code != heif_error_Ok) {
    fprintf(stderr, "%s: %s\n", action, error.message);
    exit(1);
  }
}

static heif_image* create_frame(void)
{
  heif_image* image = NULL;
  heif_error error = heif_image_create(128, 129, heif_colorspace_YCbCr,
                                       heif_chroma_420, &image);
  fail(error, "create image");

  error = heif_image_add_plane(image, heif_channel_Y, 128, 129, 8);
  fail(error, "add Y plane");
  error = heif_image_add_plane(image, heif_channel_Cb, 64, 65, 8);
  fail(error, "add Cb plane");
  uint32_t padding_id = 0;
  error = heif_image_add_component(image, 128, 129,
                                   heif_cmpd_component_type_padded,
                                   heif_component_datatype_unsigned_integer,
                                   8, &padding_id);
  fail(error, "add padding component");

  error = heif_image_add_plane(image, heif_channel_Cr, 64, 65, 8);
  fail(error, "add Cr plane");

  int y_stride = 0;
  uint8_t* y = heif_image_get_plane(image, heif_channel_Y, &y_stride);
  for (int row = 0; row < 129; row++) {
    memset(y + row * y_stride, 0x40 + row, 128);
  }

  int chroma_stride = 0;
  uint8_t* cb = heif_image_get_plane(image, heif_channel_Cb, &chroma_stride);
  for (int row = 0; row < 65; row++) {
    memset(cb + row * chroma_stride, 0x60 + row, 64);
  }
  uint8_t* cr = heif_image_get_plane(image, heif_channel_Cr, &chroma_stride);
  for (int row = 0; row < 65; row++) {
    memset(cr + row * chroma_stride, 0x90 + row, 64);
  }

  size_t padding_stride = 0;
  uint8_t* padding = heif_image_get_component(image, padding_id, &padding_stride);
  for (int row = 0; row < 129; row++) {
    memset(padding + row * padding_stride, 0x20 + row, 128);
  }

  heif_image_set_duration(image, 1);
  return image;
}

static void write_sequence(const char* path)
{
  heif_context* context = heif_context_alloc();
  heif_encoder* encoder = NULL;
  heif_error error = heif_context_get_encoder_for_format(
      context, heif_compression_uncompressed, &encoder);
  fail(error, "get encoder");

  heif_image* image = create_frame();
  heif_sequence_encoding_options* options =
      heif_sequence_encoding_options_alloc();
  heif_track* track = NULL;

  error = heif_context_add_visual_sequence_track(
      context, 128, 129, heif_track_type_image_sequence, NULL, options, &track);
  fail(error, "add track");
  error = heif_track_encode_sequence_image(track, image, encoder, options);
  fail(error, "encode frame");
  error = heif_track_encode_end_of_sequence(track, encoder);
  fail(error, "end sequence");
  error = heif_context_write_to_file(context, path);
  fail(error, "write file");

  heif_image_release(image);
  heif_track_release(track);
  heif_sequence_encoding_options_release(options);
  heif_encoder_release(encoder);
  heif_context_free(context);
}

int main(int argc, char** argv)
{
  const char* path = argc > 1 ? argv[1] : "/tmp/libheif-newbug.heif";
  write_sequence(path);

  heif_context* context = heif_context_alloc();
  heif_error error = heif_context_read_from_file(context, path, NULL);
  fail(error, "read file");

  heif_track* track = heif_context_get_track(context, 0);
  if (!track) {
    fprintf(stderr, "no visual track\n");
    return 1;
  }

  heif_image* image = NULL;
  error = heif_track_decode_next_image(
      track, &image, heif_colorspace_RGB, heif_chroma_interleaved_RGB, NULL);
  fail(error, "decode frame");

  int stride = 0;
  const uint8_t* rgb = heif_image_get_plane_readonly(
      image, heif_channel_interleaved, &stride);
  if (!rgb) {
    fprintf(stderr, "no RGB plane\n");
    return 1;
  }

  printf("%d\n", rgb[0]);
  heif_image_release(image);
  heif_track_release(track);
  heif_context_free(context);
  return 0;
}
