#include <libheif/heif.h>
#include <libheif/heif_sequences.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buffer {
  uint8_t *data;
  size_t size;
};

static heif_error write_data(heif_context *ctx, const void *data, size_t size, void *userdata)
{
  struct buffer *buffer = userdata;
  uint8_t *next = realloc(buffer->data, buffer->size + size);
  if (!next) {
    return (heif_error) {heif_error_Memory_allocation_error, heif_suberror_Unspecified, "allocation failed"};
  }
  buffer->data = next;
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
  return (heif_error) {heif_error_Ok, heif_suberror_Unspecified, NULL};
}

static int check(heif_error error, const char *where)
{
  if (error.code == heif_error_Ok) {
    return 1;
  }
  fprintf(stderr, "%s: %s\n", where, error.message);
  return 0;
}

int main(int argc, char **argv)
{
  const int width = 128;
  const int chroma420 = argc > 1 && strcmp(argv[1], "420") == 0;
  const int height = chroma420 ? 129 : 128;
  const int chroma_height = chroma420 ? (height + 1) / 2 : height;
  heif_context *writing = heif_context_alloc();
  heif_image *input = NULL;
  if (!check(heif_image_create(width, height, heif_colorspace_YCbCr, chroma420 ? heif_chroma_420 : heif_chroma_422, &input), "create image")) return 2;
  if (!check(heif_image_add_plane(input, heif_channel_Y, width, height, 8), "Y plane")) return 2;
  if (!check(heif_image_add_plane(input, heif_channel_Cb, width / 2, chroma_height, 8), "Cb plane")) return 2;
  if (!check(heif_image_add_plane(input, heif_channel_Cr, width / 2, chroma_height, 8), "Cr plane")) return 2;
  heif_color_profile_nclx *profile = heif_nclx_color_profile_alloc();
  profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
  profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_709_5;
  profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
  profile->full_range_flag = 1;
  if (!check(heif_image_set_nclx_color_profile(input, profile), "color profile")) return 2;
  heif_nclx_color_profile_free(profile);

  const heif_channel channels[] = {heif_channel_Y, heif_channel_Cb, heif_channel_Cr};
  for (int i = 0; i < 3; i++) {
    int stride = 0;
    uint8_t *plane = heif_image_get_plane(input, channels[i], &stride);
    for (int y = 0; y < (i == 0 ? height : chroma_height); y++) {
      memset(plane + y * stride, i == 0 ? 128 : 96, i == 0 ? width : width / 2);
    }
  }

  heif_encoder *encoder = NULL;
  if (!check(heif_context_get_encoder_for_format(writing, heif_compression_uncompressed, &encoder), "encoder")) return 2;
  heif_track *track = NULL;
  heif_sequence_encoding_options *options = heif_sequence_encoding_options_alloc();
  if (!check(heif_context_add_visual_sequence_track(writing, width, height, heif_track_type_image_sequence, NULL, options, &track), "add track")) return 2;
  heif_image_set_duration(input, 1000);
  if (!check(heif_track_encode_sequence_image(track, input, encoder, options), "encode frame")) return 2;
  if (!check(heif_track_encode_end_of_sequence(track, encoder), "end sequence")) return 2;

  struct buffer file = {NULL, 0};
  heif_writer writer = {1, write_data};
  if (!check(heif_context_write(writing, &writer, &file), "write file")) return 2;
  heif_track_release(track);
  heif_encoder_release(encoder);
  heif_sequence_encoding_options_release(options);
  heif_image_release(input);
  heif_context_free(writing);

  heif_context *reading = heif_context_alloc();
  if (!check(heif_context_read_from_memory(reading, file.data, file.size, NULL), "read file")) return 2;
  free(file.data);
  track = heif_context_get_track(reading, 0);
  if (!track) return 2;
  heif_image *output = NULL;
  if (!check(heif_track_decode_next_image(track, &output,
                                           chroma420 ? heif_colorspace_RGB : heif_colorspace_undefined,
                                           chroma420 ? heif_chroma_interleaved_RGB : heif_chroma_undefined,
                                           NULL), "decode frame")) return 2;
  heif_image_release(output);
  heif_track_release(track);
  heif_context_free(reading);
  return 0;
}
