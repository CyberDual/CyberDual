/*
 * libheif evaluation probe.
 *
 * Drives the public C API on an in-contract input: a well-formed HEIF file with a
 * single 'unci' (ISO/IEC 23001-17 uncompressed) image item, YCbCr 4:2:0, with
 * per-component bit depths 16/16/8 (Y/Cb/Cr) as the format allows, decoded to a
 * high-bit-depth interleaved RGB target (heif_chroma_interleaved_RRGGBB_LE).
 *
 * On a tree where the colour-conversion planner lets such an image through to
 * Op_YCbCr420_to_RRGGBBaa, that operator reads the 8-bit Cr plane through a
 * uint16_t* and runs past the end of the plane: a heap out-of-bounds read that
 * AddressSanitizer aborts on. On a tree that declines the conversion, the decode
 * simply returns an error and this program exits 0.
 */

#include "libheif/heif.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Default AddressSanitizer options for this probe: trap with abort() (SIGABRT)
 * rather than _exit(1) so the failure mode is unambiguous, and leave leak
 * detection off — the probe tests for an out-of-bounds read, and leaks are a
 * different defect class. Only consulted when the binary is ASan-instrumented;
 * an ASAN_OPTIONS environment setting still overrides these defaults. */
const char* __asan_default_options(void);
const char* __asan_default_options(void)
{
  return "abort_on_error=1:detect_leaks=0";
}

/* --- minimal box builder (static buffers, no allocation) ------------------ */

#define MAX_FILE_SIZE (2u * 1024u * 1024u)

struct buf { uint8_t data[MAX_FILE_SIZE]; size_t size; };

static void buf_u8(struct buf* b, uint8_t v)
{
  b->data[b->size++] = v;
}

static void buf_bytes(struct buf* b, const uint8_t* p, size_t n)
{
  memcpy(b->data + b->size, p, n);
  b->size += n;
}

static void buf_u16be(struct buf* b, uint16_t v)
{
  buf_u8(b, (uint8_t)(v >> 8));
  buf_u8(b, (uint8_t)(v & 0xff));
}

static void buf_u32be(struct buf* b, uint32_t v)
{
  buf_u16be(b, (uint16_t)(v >> 16));
  buf_u16be(b, (uint16_t)(v & 0xffff));
}

static void buf_fourcc(struct buf* b, const char* cc)
{
  buf_bytes(b, (const uint8_t*)cc, 4);
}

/* Append a box with the given type and payload. */
static void buf_box(struct buf* b, const char* type, const struct buf* payload, int full, int version)
{
  size_t payload_size = payload ? payload->size : 0;
  size_t header = full ? 12 : 8;
  buf_u32be(b, (uint32_t)(header + payload_size));
  buf_fourcc(b, type);
  if (full) {
    buf_u8(b, (uint8_t)version);
    buf_u8(b, 0);
    buf_u8(b, 0);
    buf_u8(b, 0);
  }
  if (payload && payload->size) {
    buf_bytes(b, payload->data, payload->size);
  }
}

/* --- the 'unci' test file ------------------------------------------------ */

/* A 512x512 image is large enough that doubling the per-sample read width of a
 * 256-wide chroma plane runs past the plane's stride-padded allocation. */
#define WIDTH 512u
#define HEIGHT 512u
#define CHROMA_WIDTH (WIDTH / 2u)
#define CHROMA_HEIGHT (HEIGHT / 2u)

/* Y = 16 bit, Cb = 16 bit, Cr = 8 bit: the depths that 'unci' may declare per
 * component. The two deeper planes are stored with two bytes per sample, the
 * Cr plane with one byte per sample. */
static const uint8_t component_bit_depths[3] = {16, 16, 8};

static void build_unci_file(struct buf* file)
{
  static struct buf ftyp, hdlr, pitm, infe, iinf, ispe, cmpd, uncC, colr;
  static struct buf ipco, ipma, iprp, idat, iloc, meta;

  buf_fourcc(&ftyp, "mif1");
  buf_u32be(&ftyp, 0);
  buf_fourcc(&ftyp, "mif1");
  buf_fourcc(&ftyp, "heic");

  buf_u32be(&hdlr, 0);
  buf_fourcc(&hdlr, "pict");
  buf_u32be(&hdlr, 0);
  buf_u32be(&hdlr, 0);
  buf_u32be(&hdlr, 0);
  buf_u8(&hdlr, 0);

  buf_u16be(&pitm, 1);

  buf_u16be(&infe, 1);  /* item_ID */
  buf_u16be(&infe, 0);  /* protection */
  buf_fourcc(&infe, "unci");
  buf_u8(&infe, 0);     /* empty item name */

  buf_u16be(&iinf, 1);
  buf_box(&iinf, "infe", &infe, 1, 2);

  buf_u32be(&ispe, WIDTH);
  buf_u32be(&ispe, HEIGHT);

  buf_u32be(&cmpd, 3);
  buf_u16be(&cmpd, 1);  /* Y */
  buf_u16be(&cmpd, 2);  /* Cb */
  buf_u16be(&cmpd, 3);  /* Cr */

  buf_u32be(&uncC, 0);  /* profile: not used */
  buf_u32be(&uncC, 3);  /* component_count */
  for (uint16_t idx = 0; idx < 3; idx++) {
    buf_u16be(&uncC, idx);
    buf_u8(&uncC, (uint8_t)(component_bit_depths[idx] - 1));
    buf_u8(&uncC, 0);   /* component_format: unsigned */
    buf_u8(&uncC, 0);   /* component_align_size */
  }
  buf_u8(&uncC, 2);     /* sampling_type: 4:2:0 */
  buf_u8(&uncC, 2);     /* interleave_type: mixed */
  buf_u8(&uncC, 0);     /* block_size */
  buf_u8(&uncC, 0);     /* flags */
  buf_u32be(&uncC, 0);  /* pixel_size */
  buf_u32be(&uncC, 0);  /* row_align_size */
  buf_u32be(&uncC, 0);  /* tile_align_size */
  buf_u32be(&uncC, 0);  /* num_tile_cols_minus_one */
  buf_u32be(&uncC, 0);  /* num_tile_rows_minus_one */

  buf_fourcc(&colr, "nclx");
  buf_u16be(&colr, 6);  /* colour_primaries: smpte170m */
  buf_u16be(&colr, 6);  /* transfer_characteristics: smpte170m */
  buf_u16be(&colr, 6);  /* matrix_coefficients: BT.601 */
  buf_u8(&colr, 0x80);  /* full_range_flag */

  buf_box(&ipco, "ispe", &ispe, 1, 0);
  buf_box(&ipco, "cmpd", &cmpd, 0, 0);
  buf_box(&ipco, "uncC", &uncC, 1, 0);
  buf_box(&ipco, "colr", &colr, 0, 0);

  buf_u32be(&ipma, 1);          /* entry_count */
  buf_u16be(&ipma, 1);          /* item_ID */
  buf_u8(&ipma, 4);             /* association_count */
  buf_u8(&ipma, 0x80 | 1);      /* essential: ispe */
  buf_u8(&ipma, 0x80 | 2);      /* essential: cmpd */
  buf_u8(&ipma, 0x80 | 3);      /* essential: uncC */
  buf_u8(&ipma, 4);             /* colr */

  buf_box(&iprp, "ipco", &ipco, 0, 0);
  buf_box(&iprp, "ipma", &ipma, 1, 0);

  /* Tile data: the luma plane (two bytes per sample, big endian), then the
   * interleaved chroma block with Cb (two bytes) and Cr (one byte) per chroma
   * position. */
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      buf_u16be(&idat, (uint16_t)(0x1000 + x + WIDTH * y));
    }
  }
  for (uint32_t y = 0; y < CHROMA_HEIGHT; y++) {
    for (uint32_t x = 0; x < CHROMA_WIDTH; x++) {
      buf_u16be(&idat, (uint16_t)(0x4000 + y * CHROMA_WIDTH + x));  /* Cb */
      buf_u8(&idat, (uint8_t)(0x80 + y * CHROMA_WIDTH + x));        /* Cr */
    }
  }

  buf_u16be(&iloc, (4 << 12) | (4 << 8));  /* offset_size=4, length_size=4 */
  buf_u16be(&iloc, 1);      /* item_count */
  buf_u16be(&iloc, 1);      /* item_ID */
  buf_u16be(&iloc, 0x0001); /* construction_method: idat */
  buf_u16be(&iloc, 0);      /* data_reference_index */
  buf_u16be(&iloc, 1);      /* extent_count */
  buf_u32be(&iloc, 0);      /* extent_offset */
  buf_u32be(&iloc, (uint32_t)idat.size);

  buf_box(&meta, "hdlr", &hdlr, 1, 0);
  buf_box(&meta, "pitm", &pitm, 1, 0);
  buf_box(&meta, "iinf", &iinf, 1, 0);
  buf_box(&meta, "iprp", &iprp, 0, 0);
  buf_box(&meta, "iloc", &iloc, 1, 1);
  buf_box(&meta, "idat", &idat, 0, 0);

  buf_box(file, "ftyp", &ftyp, 0, 0);
  buf_box(file, "meta", &meta, 1, 0);
}

int main(void)
{
  static struct buf file;

  build_unci_file(&file);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) return 2;

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read: %s\n", err.message);
    heif_context_free(ctx);
    return 2;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "handle: %s\n", err.message);
    heif_context_free(ctx);
    return 2;
  }

  /* Decode to a high-bit-depth interleaved RGB target. A tree that declines the
   * conversion of mixed-depth planes returns an error here and the probe exits
   * cleanly; a tree that routes it through Op_YCbCr420_to_RRGGBBaa over-reads
   * the one-byte-per-sample Cr plane inside this call. */
  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBB_LE, NULL);
  if (err.code != heif_error_Ok) {
    printf("decode refused cleanly: %d/%d (%s)\n", err.code, err.subcode, err.message);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return 0;
  }

  printf("decode succeeded\n");
  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
