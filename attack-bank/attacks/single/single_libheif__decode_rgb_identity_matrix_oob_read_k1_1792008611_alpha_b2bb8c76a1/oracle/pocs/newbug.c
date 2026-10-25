#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Planar RGB, component-interleaved, with a wider green channel than R/B.
   Decoding to YCbCr is a normal colorspace request. */

enum { kWidth = 64, kHeight = 64 };

static int die(const char* what, struct heif_error err)
{
  fprintf(stderr, "%s: %s\n", what, err.message ? err.message : "(no message)");
  return 1;
}

static void fill_u8(struct heif_image* img, enum heif_channel ch, uint8_t v)
{
  int stride = 0;
  uint8_t* p = heif_image_get_plane(img, ch, &stride);
  for (int y = 0; y < kHeight; y++) {
    memset(p + (size_t) y * (size_t) stride, v, kWidth);
  }
}

static void fill_u16(struct heif_image* img, enum heif_channel ch, uint16_t v)
{
  int stride = 0;
  uint16_t* p = (uint16_t*) heif_image_get_plane(img, ch, &stride);
  int samples = stride / (int) sizeof(uint16_t);
  for (int y = 0; y < kHeight; y++) {
    for (int x = 0; x < kWidth; x++) {
      p[y * samples + x] = v;
    }
  }
}

int main(void)
{
  struct heif_image* img = NULL;
  struct heif_error err = heif_image_create(kWidth, kHeight,
                                             heif_colorspace_RGB,
                                             heif_chroma_444,
                                             &img);
  if (err.code != heif_error_Ok) return die("heif_image_create", err);

  err = heif_image_add_plane(img, heif_channel_R, kWidth, kHeight, 8);
  if (err.code != heif_error_Ok) return die("add R", err);
  err = heif_image_add_plane(img, heif_channel_G, kWidth, kHeight, 16);
  if (err.code != heif_error_Ok) return die("add G", err);
  err = heif_image_add_plane(img, heif_channel_B, kWidth, kHeight, 8);
  if (err.code != heif_error_Ok) return die("add B", err);

  fill_u8(img, heif_channel_R, 32);
  fill_u16(img, heif_channel_G, 4000);
  fill_u8(img, heif_channel_B, 64);

  struct heif_context* ctx = heif_context_alloc();
  struct heif_encoder* enc = NULL;
  err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &enc);
  if (err.code != heif_error_Ok) return die("encoder", err);

  struct heif_image_handle* handle = NULL;
  err = heif_context_encode_image(ctx, img, enc, NULL, &handle);
  if (err.code != heif_error_Ok) return die("encode", err);

  const char* path = "/tmp/poc_rgb_g16.heif";
  err = heif_context_write_to_file(ctx, path);
  if (err.code != heif_error_Ok) return die("write", err);

  heif_image_handle_release(handle);
  handle = NULL;
  heif_encoder_release(enc);
  heif_image_release(img);
  heif_context_free(ctx);

  ctx = heif_context_alloc();
  err = heif_context_read_from_file(ctx, path, NULL);
  if (err.code != heif_error_Ok) return die("read", err);

  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) return die("primary", err);

  struct heif_image* decoded = NULL;
  err = heif_decode_image(handle, &decoded,
                          heif_colorspace_YCbCr,
                          heif_chroma_444,
                          NULL);
  if (err.code != heif_error_Ok) return die("heif_decode_image", err);

  int yw = heif_image_get_width(decoded, heif_channel_Y);
  int yh = heif_image_get_height(decoded, heif_channel_Y);
  printf("decoded Y %dx%d\n", yw, yh);

  heif_image_release(decoded);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
