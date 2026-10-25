#define _POSIX_C_SOURCE 200809L

#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
  const int width = 128;
  const int height = 64;
  const heif_channel channels[3] = {heif_channel_R, heif_channel_G, heif_channel_B};
  const int depths[3] = {12, 8, 8};
  heif_image *source = NULL, *decoded = NULL;
  heif_context *writer = NULL, *reader = NULL;
  heif_encoder *encoder = NULL;
  heif_image_handle *handle = NULL;
  char path[] = "/tmp/libheif-probe-XXXXXX";
  int fd = -1;
  int result = 1;
  heif_error err;

  err = heif_image_create(width, height, heif_colorspace_RGB, heif_chroma_444, &source);
  if (err.code != heif_error_Ok) goto done;
  for (int c = 0; c < 3; ++c) {
    err = heif_image_add_plane(source, channels[c], width, height, depths[c]);
    if (err.code != heif_error_Ok) goto done;
    size_t stride = 0;
    uint8_t *plane = heif_image_get_plane2(source, channels[c], &stride);
    if (!plane) goto done;
    for (int y = 0; y < height; ++y) {
      if (depths[c] > 8) {
        uint16_t *row = (uint16_t *)(plane + (size_t)y * stride);
        for (int x = 0; x < width; ++x) row[x] = (uint16_t)(1000 + c);
      } else {
        memset(plane + (size_t)y * stride, 64 + c, width);
      }
    }
  }

  writer = heif_context_alloc();
  reader = heif_context_alloc();
  if (!writer || !reader) goto done;
  err = heif_context_get_encoder_for_format(writer, heif_compression_uncompressed, &encoder);
  if (err.code != heif_error_Ok) goto done;
  err = heif_context_encode_image(writer, source, encoder, NULL, NULL);
  if (err.code != heif_error_Ok) goto done;

  fd = mkstemp(path);
  if (fd < 0) goto done;
  close(fd);
  fd = -1;
  err = heif_context_write_to_file(writer, path);
  if (err.code != heif_error_Ok) goto done;
  err = heif_context_read_from_file(reader, path, NULL);
  if (err.code != heif_error_Ok) goto done;
  err = heif_context_get_primary_image_handle(reader, &handle);
  if (err.code != heif_error_Ok) goto done;
  err = heif_decode_image(handle, &decoded, heif_colorspace_YCbCr, heif_chroma_420, NULL);
  if (err.code == heif_error_Unsupported_feature) {
    result = 0;
    goto done;
  }
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode: %s\n", err.message);
    goto done;
  }
  result = 0;

done:
  if (fd >= 0) close(fd);
  unlink(path);
  if (decoded) heif_image_release(decoded);
  if (handle) heif_image_handle_release(handle);
  if (encoder) heif_encoder_release(encoder);
  if (reader) heif_context_free(reader);
  if (writer) heif_context_free(writer);
  if (source) heif_image_release(source);
  return result;
}
