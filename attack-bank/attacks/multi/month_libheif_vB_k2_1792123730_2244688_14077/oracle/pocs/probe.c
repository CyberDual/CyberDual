/*
 * Probe: decode an HEIF image stored with the ISO/IEC 23001-17 uncompressed
 * codec ('unci') to 16-bit interleaved RGB through the public libheif API.
 *
 * The file is generated in memory below: a single 'unci' item, 512x512,
 * YCbCr 4:2:0, mixed interleave, with a 16-bit luma component and 8-bit
 * chroma components (per-component bit depths are part of the 'unci'
 * component configuration).
 *
 * Expected behavior: the decode either succeeds or fails with a clean
 * heif_error. Any other outcome (crash, sanitizer abort) is a library fault.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libheif/heif.h>

/* When run under AddressSanitizer, abort (SIGABRT) on a detected memory
 * error so the failure is unmistakable. This is the ASan runtime's public
 * options hook; in a non-instrumented binary it is an unused function and
 * has no effect. */
const char* __asan_default_options(void)
{
  return "abort_on_error=1";
}

#define WIDTH 512
#define HEIGHT 512
#define CHROMA_WIDTH (WIDTH / 2)
#define CHROMA_HEIGHT (HEIGHT / 2)

typedef struct {
  uint8_t* data;
  size_t len;
  size_t cap;
} buf;

static void buf_reserve(buf* b, size_t extra)
{
  if (b->len + extra > b->cap) {
    size_t ncap = (b->cap == 0) ? 4096 : b->cap;
    while (ncap < b->len + extra) {
      ncap *= 2;
    }
    b->data = (uint8_t*) realloc(b->data, ncap);
    if (!b->data) {
      fprintf(stderr, "out of memory\n");
      exit(2);
    }
    b->cap = ncap;
  }
}

static void put_u8(buf* b, uint8_t v)
{
  buf_reserve(b, 1);
  b->data[b->len++] = v;
}

static void put_u16be(buf* b, uint16_t v)
{
  put_u8(b, (uint8_t) (v >> 8));
  put_u8(b, (uint8_t) (v & 0xff));
}

static void put_u32be(buf* b, uint32_t v)
{
  put_u8(b, (uint8_t) (v >> 24));
  put_u8(b, (uint8_t) ((v >> 16) & 0xff));
  put_u8(b, (uint8_t) ((v >> 8) & 0xff));
  put_u8(b, (uint8_t) (v & 0xff));
}

static void put_fourcc(buf* b, const char* cc)
{
  buf_reserve(b, 4);
  memcpy(b->data + b->len, cc, 4);
  b->len += 4;
}

static void put_bytes(buf* b, const uint8_t* d, size_t n)
{
  buf_reserve(b, n);
  memcpy(b->data + b->len, d, n);
  b->len += n;
}

/* Appends a box header with a placeholder size; returns the payload start.
 * Call box_close() once the payload has been written. */
static size_t box_open(buf* b, const char* fourcc)
{
  put_u32be(b, 0);
  put_fourcc(b, fourcc);
  return b->len;
}

static void box_close(buf* b, size_t payload_start)
{
  uint32_t size = (uint32_t) (b->len - (payload_start - 8));
  b->data[payload_start - 8] = (uint8_t) (size >> 24);
  b->data[payload_start - 7] = (uint8_t) ((size >> 16) & 0xff);
  b->data[payload_start - 6] = (uint8_t) ((size >> 8) & 0xff);
  b->data[payload_start - 5] = (uint8_t) (size & 0xff);
}

static size_t full_box_open(buf* b, const char* fourcc, uint8_t version, uint32_t flags)
{
  size_t start = box_open(b, fourcc);
  put_u8(b, version);
  put_u8(b, (uint8_t) ((flags >> 16) & 0xff));
  put_u8(b, (uint8_t) ((flags >> 8) & 0xff));
  put_u8(b, (uint8_t) (flags & 0xff));
  return start;
}

static uint8_t* build_unci_file(size_t* out_size)
{
  buf f = {NULL, 0, 0};

  /* ftyp */
  {
    size_t s = box_open(&f, "ftyp");
    put_fourcc(&f, "mif1");
    put_u32be(&f, 0);
    put_fourcc(&f, "mif1");
    put_fourcc(&f, "heic");
    box_close(&f, s);
  }

  size_t meta = full_box_open(&f, "meta", 0, 0);

  /* hdlr */
  {
    size_t s = full_box_open(&f, "hdlr", 0, 0);
    put_u32be(&f, 0);
    put_fourcc(&f, "pict");
    put_u32be(&f, 0);
    put_u32be(&f, 0);
    put_u32be(&f, 0);
    put_u8(&f, 0);
    box_close(&f, s);
  }

  /* pitm: item 1 */
  {
    size_t s = full_box_open(&f, "pitm", 0, 0);
    put_u16be(&f, 1);
    box_close(&f, s);
  }

  /* iinf: one 'unci' item */
  {
    size_t s = full_box_open(&f, "iinf", 0, 0);
    put_u16be(&f, 1);
    size_t infe = full_box_open(&f, "infe", 2, 0);
    put_u16be(&f, 1);
    put_u16be(&f, 0);
    put_fourcc(&f, "unci");
    put_u8(&f, 0);
    box_close(&f, infe);
    box_close(&f, s);
  }

  /* iprp */
  {
    size_t iprp = box_open(&f, "iprp");

    size_t ipco = box_open(&f, "ipco");

    /* ispe */
    {
      size_t s = full_box_open(&f, "ispe", 0, 0);
      put_u32be(&f, WIDTH);
      put_u32be(&f, HEIGHT);
      box_close(&f, s);
    }

    /* cmpd: Y, Cb, Cr */
    {
      size_t s = box_open(&f, "cmpd");
      put_u32be(&f, 3);
      put_u16be(&f, 1);
      put_u16be(&f, 2);
      put_u16be(&f, 3);
      box_close(&f, s);
    }

    /* uncC: mixed interleave, 4:2:0 sampling, Y=16 bit, Cb=8 bit, Cr=8 bit */
    {
      static const uint8_t depths[3] = {16, 8, 8};
      size_t s = full_box_open(&f, "uncC", 0, 0);
      put_u32be(&f, 0); /* profile */
      put_u32be(&f, 3); /* component_count */
      for (uint16_t i = 0; i < 3; i++) {
        put_u16be(&f, i);                      /* component_index */
        put_u8(&f, (uint8_t) (depths[i] - 1)); /* component_bit_depth_minus_one */
        put_u8(&f, 0);                         /* component_format */
        put_u8(&f, 0);                         /* component_align_size */
      }
      put_u8(&f, 2);    /* sampling_type: 4:2:0 */
      put_u8(&f, 2);    /* interleave_type: mixed */
      put_u8(&f, 0);    /* block_size */
      put_u8(&f, 0);    /* flags */
      put_u32be(&f, 0); /* pixel_size */
      put_u32be(&f, 0); /* row_align_size */
      put_u32be(&f, 0); /* tile_align_size */
      put_u32be(&f, 0); /* num_tile_cols_minus_one */
      put_u32be(&f, 0); /* num_tile_rows_minus_one */
      box_close(&f, s);
    }

    /* colr (nclx): BT.601 matrix, full range */
    {
      size_t s = box_open(&f, "colr");
      put_fourcc(&f, "nclx");
      put_u16be(&f, 6);
      put_u16be(&f, 6);
      put_u16be(&f, 6);
      put_u8(&f, 0x80);
      box_close(&f, s);
    }

    box_close(&f, ipco);

    /* ipma: ispe, cmpd, uncC essential; colr not */
    {
      size_t s = full_box_open(&f, "ipma", 0, 0);
      put_u32be(&f, 1); /* entry_count */
      put_u16be(&f, 1); /* item_ID */
      put_u8(&f, 4);    /* association_count */
      put_u8(&f, 0x80 | 1);
      put_u8(&f, 0x80 | 2);
      put_u8(&f, 0x80 | 3);
      put_u8(&f, 4);
      box_close(&f, s);
    }

    box_close(&f, iprp);
  }

  /* iloc (version 1): item data stored in 'idat' (construction_method 1) */
  {
    size_t s = full_box_open(&f, "iloc", 1, 0);
    put_u16be(&f, (uint16_t) ((4 << 12) | (4 << 8))); /* offset_size=4, length_size=4 */
    put_u16be(&f, 1); /* item_count */
    put_u16be(&f, 1); /* item_ID */
    put_u16be(&f, 1); /* construction_method = idat */
    put_u16be(&f, 0); /* data_reference_index */
    put_u16be(&f, 1); /* extent_count */
    put_u32be(&f, 0); /* extent_offset */
    put_u32be(&f, (uint32_t) (WIDTH * HEIGHT * 2 + CHROMA_WIDTH * CHROMA_HEIGHT * 2));
    box_close(&f, s);
  }

  /* idat: Y plane (2 bytes/sample, big-endian), then interleaved Cb/Cr */
  {
    size_t s = box_open(&f, "idat");
    for (uint32_t y = 0; y < HEIGHT; y++) {
      for (uint32_t x = 0; x < WIDTH; x++) {
        put_u16be(&f, (uint16_t) (0x1000 + x + WIDTH * y));
      }
    }
    for (uint32_t y = 0; y < CHROMA_HEIGHT; y++) {
      for (uint32_t x = 0; x < CHROMA_WIDTH; x++) {
        put_u8(&f, (uint8_t) (0x40 + y * CHROMA_WIDTH + x));
        put_u8(&f, (uint8_t) (0x80 + y * CHROMA_WIDTH + x));
      }
    }
    box_close(&f, s);
  }

  box_close(&f, meta);

  *out_size = f.len;
  return f.data;
}

int main(void)
{
  size_t file_size = 0;
  uint8_t* file = build_unci_file(&file_size);

  struct heif_context* ctx = heif_context_alloc();
  if (ctx == NULL) {
    fprintf(stderr, "heif_context_alloc failed\n");
    free(file);
    return 2;
  }

  struct heif_error err = heif_context_read_from_memory_without_copy(ctx, file, file_size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_context_read_from_memory: %s\n", err.message);
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  struct heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "heif_context_get_primary_image_handle: %s\n", err.message);
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  struct heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBB_LE, NULL);
  if (err.code != heif_error_Ok) {
    /* A clean refusal is a fine outcome. */
    printf("decode refused cleanly: %s\n", err.message);
  }
  else {
    printf("decoded: %d x %d\n",
           heif_image_get_width(img, heif_channel_interleaved),
           heif_image_get_height(img, heif_channel_interleaved));
    heif_image_release(img);
  }

  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file);
  return 0;
}
