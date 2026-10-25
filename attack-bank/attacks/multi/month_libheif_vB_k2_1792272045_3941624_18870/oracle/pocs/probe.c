#include "libheif/heif.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void report(const char* where, heif_error error)
{
  fprintf(stderr, "%s: %d/%d: %s\n", where, (int)error.code,
          (int)error.subcode, error.message ? error.message : "(null)");
}

int main(void)
{
  const int width = 512;
  const int height = 512;
  const int chroma_width = width / 2;
  const int chroma_height = height / 2;

  heif_image* image = NULL;
  heif_error error = heif_image_create(width, height, heif_colorspace_YCbCr,
                                       heif_chroma_420, &image);
  if (error.code != heif_error_Ok) {
    report("create image", error);
    return 1;
  }

  const struct {
    heif_channel channel;
    int width;
    int height;
    int bit_depth;
  } planes[] = {
      {heif_channel_Y, width, height, 10},
      {heif_channel_Cb, chroma_width, chroma_height, 8},
      {heif_channel_Cr, chroma_width, chroma_height, 8},
  };

  for (size_t plane_index = 0;
       plane_index < sizeof(planes) / sizeof(planes[0]);
       plane_index++) {
    error = heif_image_add_plane(image, planes[plane_index].channel,
                                 planes[plane_index].width,
                                 planes[plane_index].height,
                                 planes[plane_index].bit_depth);
    if (error.code != heif_error_Ok) {
      report("add plane", error);
      return 1;
    }

    int stride = 0;
    uint8_t* data = heif_image_get_plane(image, planes[plane_index].channel,
                                         &stride);
    if (data == NULL) {
      fprintf(stderr, "plane data is null\n");
      return 1;
    }

    for (int y = 0; y < planes[plane_index].height; y++) {
      if (planes[plane_index].bit_depth > 8) {
        uint16_t* row = (uint16_t*)(data + (size_t)y * stride);
        for (int x = 0; x < planes[plane_index].width; x++) {
          row[x] = (uint16_t)(0x200 + ((x + y) & 0xff));
        }
      }
      else {
        memset(data + (size_t)y * stride, 0x80,
               (size_t)planes[plane_index].width);
      }
    }
  }

  heif_context* context = heif_context_alloc();
  if (context == NULL) {
    fprintf(stderr, "context allocation failed\n");
    return 1;
  }

  heif_encoder* encoder = NULL;
  error = heif_context_get_encoder_for_format(
      context, heif_compression_uncompressed, &encoder);
  if (error.code != heif_error_Ok) {
    report("get encoder", error);
    return 1;
  }

  heif_image_handle* handle = NULL;
  error = heif_context_encode_image(context, image, encoder, NULL, &handle);
  heif_encoder_release(encoder);
  if (error.code != heif_error_Ok) {
    report("encode image", error);
    return 1;
  }

  heif_image* decoded_image = NULL;
  error = heif_decode_image(handle, &decoded_image, heif_colorspace_RGB,
                            heif_chroma_interleaved_RRGGBB_LE, NULL);
  if (error.code != heif_error_Ok) {
    report("decode image", error);
  }

  heif_image_handle_release(handle);
  heif_image_release(decoded_image);
  heif_image_release(image);
  heif_context_free(context);

  return error.code == heif_error_Ok ? 0 : 1;
}
