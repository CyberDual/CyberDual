/*
 * Probe for the libheif maintenance window.
 *
 * Builds, through the public C API, a minimal but fully valid HEIF file:
 *
 *   - item 1: the primary 'mski' mask image, 16 bits per sample (mskC),
 *   - item 2: an 'mski' mask image, 8 bits per sample, carrying the
 *     "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha" auxC property and tied to
 *     item 1 through a single-item 'auxl' reference, so it is merged as the
 *     alpha auxiliary image of item 1,
 *   - pixel data served from 'idat' via construction_method 1,
 *   - both items share the same 128x128 'ispe'.
 *
 * The file is in-contract: every box is well formed, both mask depths (8 and
 * 16) are the ones 'mskC' allows, the data extents have the exact lengths the
 * depths require, and the alpha reference is the documented MIAF way to attach
 * an alpha auxiliary image.
 *
 * The image is then decoded with alpha composition against a solid background
 * enabled (the public heif_color_conversion_options_ext), requesting the
 * native monochrome layout.
 *
 * On the unmodified maintenance batch this decode succeeds. On the modified
 * tree the colour planes are 16-bit while the alpha plane keeps one-byte
 * storage, and the alpha compositing operator reads the alpha plane through
 * 16-bit samples past the end of its allocation: AddressSanitizer aborts with
 * a heap-buffer-overflow READ in composite_plane() (libheif/color-conversion/alpha.cc).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "libheif/heif.h"

/* ---- tiny byte-buffer helpers (plain C) ---- */

typedef struct {
  uint8_t* d;
  size_t n, cap;
} Buf;

static Buf buf_new(void)
{
  Buf b;
  b.d = NULL;
  b.n = 0;
  b.cap = 0;
  return b;
}

static void buf_u8(Buf* b, uint8_t v)
{
  if (b->n == b->cap) {
    b->cap = b->cap ? b->cap * 2 : 256;
    b->d = realloc(b->d, b->cap);
  }
  b->d[b->n++] = v;
}

static void buf_u16(Buf* b, uint16_t v)
{
  buf_u8(b, (uint8_t) (v >> 8));
  buf_u8(b, (uint8_t) (v & 0xff));
}

static void buf_u32(Buf* b, uint32_t v)
{
  buf_u16(b, (uint16_t) (v >> 16));
  buf_u16(b, (uint16_t) (v & 0xffff));
}

static void buf_4cc(Buf* b, const char* s)
{
  for (int i = 0; i < 4; i++) {
    buf_u8(b, (uint8_t) s[i]);
  }
}

static void buf_buf(Buf* b, const Buf* o)
{
  for (size_t i = 0; i < o->n; i++) {
    buf_u8(b, o->d[i]);
  }
}

/* Wraps 'payload' into a box of the given type; 'full' boxes carry the
   version/flags word. The wrapped box is appended to 'out'. */
static void box(Buf* out, const char* type, const Buf* payload, int full, int version)
{
  Buf b = buf_new();
  buf_u32(&b, 8 + payload->n + (full ? 4 : 0));
  buf_4cc(&b, type);
  if (full) {
    buf_u8(&b, (uint8_t) version);
    buf_u8(&b, 0);
    buf_u16(&b, 0);
  }
  buf_buf(&b, payload);
  buf_buf(out, &b);
  free(b.d);
}

static void free_buf(Buf* b)
{
  free(b->d);
  b->d = NULL;
  b->n = b->cap = 0;
}

/* ---- the file ---- */

#define WIDTH 128
#define HEIGHT 128
#define PRIMARY_BPP 16 /* 'mskC' of the primary mask image */
#define ALPHA_BPP 8    /* 'mskC' of the alpha mask image */

int main(void)
{
  const size_t n1 = (size_t) WIDTH * HEIGHT * (PRIMARY_BPP / 8);
  const size_t n2 = (size_t) WIDTH * HEIGHT * (ALPHA_BPP / 8);

  /* pixel data of both items: deterministic, non-degenerate patterns */
  Buf px1 = buf_new(), px2 = buf_new();
  for (size_t i = 0; i < n1; i++) {
    buf_u8(&px1, (uint8_t) ((i * 3) & 0xff));
  }
  for (size_t i = 0; i < n2; i++) {
    buf_u8(&px2, (uint8_t) ((i * 5 + 7) & 0xff));
  }

  Buf ftyp = buf_new();
  {
    Buf p = buf_new();
    buf_4cc(&p, "mif1");
    buf_u32(&p, 0);
    buf_4cc(&p, "mif1");
    buf_4cc(&p, "heic");
    box(&ftyp, "ftyp", &p, 0, 0);
    free_buf(&p);
  }

  Buf hdlr = buf_new();
  {
    Buf p = buf_new();
    buf_u32(&p, 0);
    buf_4cc(&p, "pict");
    buf_u32(&p, 0);
    buf_u32(&p, 0);
    buf_u32(&p, 0);
    buf_u8(&p, 0);
    box(&hdlr, "hdlr", &p, 1, 0);
    free_buf(&p);
  }

  Buf pitm = buf_new();
  {
    Buf p = buf_new();
    buf_u16(&p, 1); /* primary item: item 1 */
    box(&pitm, "pitm", &p, 1, 0);
    free_buf(&p);
  }

  Buf iinf = buf_new();
  {
    Buf p = buf_new();
    buf_u16(&p, 2); /* two entries */
    for (int id = 1; id <= 2; id++) {
      Buf e = buf_new();
      buf_u16(&e, (uint16_t) id);
      buf_u16(&e, 0);
      buf_4cc(&e, "mski");
      buf_u8(&e, 0);
      Buf ib = buf_new();
      box(&ib, "infe", &e, 1, 2);
      buf_buf(&p, &ib);
      free_buf(&ib);
      free_buf(&e);
    }
    box(&iinf, "iinf", &p, 1, 0);
    free_buf(&p);
  }

  Buf ispe = buf_new();
  {
    Buf p = buf_new();
    buf_u32(&p, WIDTH);
    buf_u32(&p, HEIGHT);
    box(&ispe, "ispe", &p, 1, 0);
    free_buf(&p);
  }

  Buf mskC1 = buf_new();
  {
    Buf p = buf_new();
    buf_u8(&p, PRIMARY_BPP);
    box(&mskC1, "mskC", &p, 1, 0);
    free_buf(&p);
  }

  Buf mskC2 = buf_new();
  {
    Buf p = buf_new();
    buf_u8(&p, ALPHA_BPP);
    box(&mskC2, "mskC", &p, 1, 0);
    free_buf(&p);
  }

  Buf auxC = buf_new();
  {
    Buf p = buf_new();
    const char* urn = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha";
    for (size_t i = 0; i <= strlen(urn); i++) {
      buf_u8(&p, (uint8_t) urn[i]);
    }
    box(&auxC, "auxC", &p, 1, 0);
    free_buf(&p);
  }

  /* ipco: [1] ispe, [2] mskC(16), [3] mskC(8), [4] auxC(alpha) */
  Buf ipco_payload = buf_new();
  buf_buf(&ipco_payload, &ispe);
  buf_buf(&ipco_payload, &mskC1);
  buf_buf(&ipco_payload, &mskC2);
  buf_buf(&ipco_payload, &auxC);
  Buf ipco = buf_new();
  box(&ipco, "ipco", &ipco_payload, 0, 0);

  /* item 1: ispe + mskC(16); item 2: ispe + mskC(8) + auxC(alpha) */
  Buf ipma = buf_new();
  {
    Buf p = buf_new();
    buf_u32(&p, 2); /* two associations */
    buf_u16(&p, 1);
    buf_u8(&p, 2);
    buf_u8(&p, 0x80 | 1);
    buf_u8(&p, 0x80 | 2);
    buf_u16(&p, 2);
    buf_u8(&p, 3);
    buf_u8(&p, 0x80 | 1);
    buf_u8(&p, 0x80 | 3);
    buf_u8(&p, 0x80 | 4);
    box(&ipma, "ipma", &p, 1, 0);
    free_buf(&p);
  }

  Buf iprp_payload = buf_new();
  buf_buf(&iprp_payload, &ipco);
  buf_buf(&iprp_payload, &ipma);
  Buf iprp = buf_new();
  box(&iprp, "iprp", &iprp_payload, 0, 0);

  Buf iloc = buf_new();
  {
    Buf p = buf_new();
    buf_u8(&p, (4 << 4) | 4); /* offset_size=4, length_size=4 */
    buf_u8(&p, 0);            /* base_offset_size=0, index_size=0 */
    buf_u16(&p, 2);           /* two items */
    buf_u16(&p, 1);           /* item 1 */
    buf_u16(&p, 0x0001);      /* construction_method=1: idat */
    buf_u16(&p, 0);           /* data_reference_index=0 */
    buf_u16(&p, 1);           /* one extent */
    buf_u32(&p, 0);
    buf_u32(&p, (uint32_t) n1);
    buf_u16(&p, 2);           /* item 2 */
    buf_u16(&p, 0x0001);
    buf_u16(&p, 0);
    buf_u16(&p, 1);
    buf_u32(&p, (uint32_t) n1);
    buf_u32(&p, (uint32_t) n2);
    box(&iloc, "iloc", &p, 1, 1);
    free_buf(&p);
  }

  /* item 2 is the alpha auxiliary image of item 1 */
  Buf iref = buf_new();
  {
    Buf dimg = buf_new();
    buf_u16(&dimg, 2); /* from_item_ID = 2 */
    buf_u16(&dimg, 1); /* reference_count = 1 */
    buf_u16(&dimg, 1); /* to_item_ID = 1 */
    Buf p = buf_new();
    box(&p, "auxl", &dimg, 0, 0);
    box(&iref, "iref", &p, 1, 0);
    free_buf(&p);
    free_buf(&dimg);
  }

  Buf idat_payload = buf_new();
  buf_buf(&idat_payload, &px1);
  buf_buf(&idat_payload, &px2);
  Buf idat = buf_new();
  box(&idat, "idat", &idat_payload, 0, 0);

  Buf meta_payload = buf_new();
  buf_buf(&meta_payload, &hdlr);
  buf_buf(&meta_payload, &pitm);
  buf_buf(&meta_payload, &iinf);
  buf_buf(&meta_payload, &iprp);
  buf_buf(&meta_payload, &iloc);
  buf_buf(&meta_payload, &iref);
  buf_buf(&meta_payload, &idat);
  Buf meta = buf_new();
  box(&meta, "meta", &meta_payload, 1, 0);

  Buf file = buf_new();
  buf_buf(&file, &ftyp);
  buf_buf(&file, &meta);

  /* ---- decode through the public API ---- */

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.d, file.n, NULL);
  if (err.code != heif_error_Ok) {
    printf("read: %s\n", err.message);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    printf("handle: %s\n", err.message);
    return 1;
  }

  /* Ask for alpha composition against a solid background, the documented way
     to composite an image that carries an alpha auxiliary image. */
  heif_decoding_options* opts = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0;
  ext->background_green = 0;
  ext->background_blue = 0;
  opts->color_conversion_options_ext = ext;

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_monochrome, heif_chroma_monochrome, opts);
  if (err.code != heif_error_Ok) {
    printf("decode: %s\n", err.message);
  }
  else {
    printf("decoded %dx%d monochrome\n",
           heif_image_get_primary_width(img), heif_image_get_primary_height(img));
    heif_image_release(img);
  }

  heif_decoding_options_free(opts);
  heif_color_conversion_options_ext_free(ext);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  free_buf(&px1);
  free_buf(&px2);
  free_buf(&ftyp);
  free_buf(&hdlr);
  free_buf(&pitm);
  free_buf(&iinf);
  free_buf(&ispe);
  free_buf(&mskC1);
  free_buf(&mskC2);
  free_buf(&auxC);
  free_buf(&ipco_payload);
  free_buf(&ipco);
  free_buf(&ipma);
  free_buf(&iprp_payload);
  free_buf(&iprp);
  free_buf(&iloc);
  free_buf(&iref);
  free_buf(&idat_payload);
  free_buf(&idat);
  free_buf(&meta_payload);
  free_buf(&meta);
  free_buf(&file);

  return 0;
}
