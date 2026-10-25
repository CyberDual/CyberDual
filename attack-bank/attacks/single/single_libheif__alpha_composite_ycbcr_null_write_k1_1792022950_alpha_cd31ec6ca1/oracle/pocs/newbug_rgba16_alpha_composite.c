/*
 * PoC input for evaluating alpha-plane compositing during decode.
 *
 * Builds a minimal HEIF file holding a single 'unci' item: a 64x64 planar
 * RGBA image with every component declared as 16 bit (R=G=B=Alpha=16),
 * stored plane by plane in the 'idat'. The item is then decoded through the
 * public API with alpha composition onto a solid white background requested
 * (heif_alpha_composition_mode_solid_color) and a planar 4:4:4 RGB target,
 * i.e. the ordinary "composite the transparent image onto a background while
 * converting to RGB" decode request.
 *
 * The file is fully within the decoder's input contract: it uses only the
 * built-in uncompressed codec, one item, 4:4:4 sampling, uniform component
 * bit depths and component (planar) interleave.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libheif/heif.h"

#define WIDTH 64
#define HEIGHT 64

/* --- minimal growable byte buffer --- */

typedef struct
{
  uint8_t* data;
  size_t size;
  size_t cap;
} Buf;

static void buf_reserve(Buf* b, size_t need)
{
  if (b->size + need <= b->cap) {
    return;
  }
  size_t cap = b->cap ? b->cap : 256;
  while (cap < b->size + need) {
    cap *= 2;
  }
  b->data = (uint8_t*) realloc(b->data, cap);
  if (!b->data) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }
  b->cap = cap;
}

static void buf_put(Buf* b, const void* p, size_t n)
{
  buf_reserve(b, n);
  memcpy(b->data + b->size, p, n);
  b->size += n;
}

static void put_u32_be(Buf* b, uint32_t v)
{
  uint8_t t[4] = {(uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v};
  buf_put(b, t, 4);
}

static void put_u16_be(Buf* b, uint16_t v)
{
  uint8_t t[2] = {(uint8_t) (v >> 8), (uint8_t) v};
  buf_put(b, t, 2);
}

static void put_fourcc(Buf* b, const char fourcc[4])
{
  buf_put(b, fourcc, 4);
}

static void put_cstr(Buf* b, const char* s)
{
  buf_put(b, s, strlen(s) + 1);
}

static void make_box(Buf* out, const char fourcc[4], const Buf* payload, int is_full_box, uint8_t version)
{
  Buf box = {NULL, 0, 0};
  uint32_t size = (uint32_t) (8 + payload->size + (is_full_box ? 4 : 0));

  put_u32_be(&box, size);
  put_fourcc(&box, fourcc);
  if (is_full_box) {
    put_u32_be(&box, (uint32_t) version << 24);
  }
  buf_put(&box, payload->data, payload->size);

  buf_put(out, box.data, box.size);
  free(box.data);
}

/* --- assemble the HEIF file --- */

static void build_file(Buf* file)
{
  /* ftyp */
  {
    Buf p = {NULL, 0, 0};
    put_fourcc(&p, "mif1");
    put_u32_be(&p, 0);
    put_fourcc(&p, "mif1");
    put_fourcc(&p, "heic");
    make_box(file, "ftyp", &p, 0, 0);
    free(p.data);
  }

  Buf meta = {NULL, 0, 0};

  /* hdlr */
  {
    Buf p = {NULL, 0, 0};
    put_u32_be(&p, 0);
    put_fourcc(&p, "pict");
    put_u32_be(&p, 0);
    put_u32_be(&p, 0);
    put_u32_be(&p, 0);
    buf_put(&p, "", 1);
    make_box(&meta, "hdlr", &p, 1, 0);
    free(p.data);
  }

  /* pitm */
  {
    Buf p = {NULL, 0, 0};
    put_u16_be(&p, 1);
    make_box(&meta, "pitm", &p, 1, 0);
    free(p.data);
  }

  /* iinf with one 'unci' item */
  {
    Buf infe = {NULL, 0, 0};
    put_u16_be(&infe, 1);        /* item_ID */
    put_u16_be(&infe, 0);        /* protection_index */
    put_fourcc(&infe, "unci");
    put_cstr(&infe, "");
    Buf iinf = {NULL, 0, 0};
    put_u16_be(&iinf, 1);        /* entry_count */
    make_box(&iinf, "infe", &infe, 1, 2);
    make_box(&meta, "iinf", &iinf, 1, 0);
    free(infe.data);
    free(iinf.data);
  }

  /* ispe */
  Buf ispe = {NULL, 0, 0};
  {
    Buf p = {NULL, 0, 0};
    put_u32_be(&p, WIDTH);
    put_u32_be(&p, HEIGHT);
    make_box(&ispe, "ispe", &p, 1, 0);
    free(p.data);
  }

  /* cmpd: R, G, B, Alpha */
  Buf cmpd = {NULL, 0, 0};
  {
    Buf p = {NULL, 0, 0};
    put_u32_be(&p, 4);
    put_u16_be(&p, 4);           /* red */
    put_u16_be(&p, 5);           /* green */
    put_u16_be(&p, 6);           /* blue */
    put_u16_be(&p, 7);           /* alpha */
    make_box(&cmpd, "cmpd", &p, 0, 0);
    free(p.data);
  }

  /* uncC (v0): component interleave, 4:4:4, all components 16 bit */
  Buf uncC = {NULL, 0, 0};
  {
    Buf p = {NULL, 0, 0};
    put_u32_be(&p, 0);           /* profile */
    put_u32_be(&p, 4);           /* component_count */
    for (uint16_t idx = 0; idx < 4; idx++) {
      put_u16_be(&p, idx);       /* component_index */
      buf_put(&p, "\x0f", 1);    /* component_bit_depth_minus_one = 15 */
      buf_put(&p, "\x00", 1);    /* component_format: unsigned */
      buf_put(&p, "\x00", 1);    /* component_align_size */
    }
    buf_put(&p, "\x00", 1);      /* sampling_type = 4:4:4 */
    buf_put(&p, "\x00", 1);      /* interleave_type = component (planar) */
    buf_put(&p, "\x00", 1);      /* block_size */
    buf_put(&p, "\x00", 1);      /* flags: big-endian components */
    put_u32_be(&p, 0);           /* pixel_size */
    put_u32_be(&p, 0);           /* row_align_size */
    put_u32_be(&p, 0);           /* tile_align_size */
    put_u32_be(&p, 0);           /* num_tile_cols_minus_one */
    put_u32_be(&p, 0);           /* num_tile_rows_minus_one */
    make_box(&uncC, "uncC", &p, 1, 0);
    free(p.data);
  }

  /* ipco / ipma / iprp */
  {
    Buf ipco_p = {NULL, 0, 0};
    buf_put(&ipco_p, ispe.data, ispe.size);
    buf_put(&ipco_p, cmpd.data, cmpd.size);
    buf_put(&ipco_p, uncC.data, uncC.size);
    Buf ipco = {NULL, 0, 0};
    make_box(&ipco, "ipco", &ipco_p, 0, 0);

    Buf ipma_p = {NULL, 0, 0};
    put_u32_be(&ipma_p, 1);            /* entry_count */
    put_u16_be(&ipma_p, 1);            /* item_ID */
    buf_put(&ipma_p, "\x03", 1);       /* association_count */
    buf_put(&ipma_p, "\x81", 1);       /* essential, ispe */
    buf_put(&ipma_p, "\x82", 1);       /* essential, cmpd */
    buf_put(&ipma_p, "\x83", 1);       /* essential, uncC */
    Buf ipma = {NULL, 0, 0};
    make_box(&ipma, "ipma", &ipma_p, 1, 0);

    Buf iprp_p = {NULL, 0, 0};
    buf_put(&iprp_p, ipco.data, ipco.size);
    buf_put(&iprp_p, ipma.data, ipma.size);
    make_box(&meta, "iprp", &iprp_p, 0, 0);
    free(ipma_p.data);
    free(ipma.data);
    free(iprp_p.data);
    free(ipco.data);
    free(ipco_p.data);
  }
  free(ispe.data);
  free(cmpd.data);
  free(uncC.data);

  /* idat: planar tile data, R, G, B, Alpha, 2 bytes per sample, big-endian */
  Buf idat = {NULL, 0, 0};
  {
    Buf p = {NULL, 0, 0};
    for (uint32_t plane = 0; plane < 4; plane++) {
      for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
          uint16_t v;
          if (plane == 3) {
            v = 0xFF00;                     /* nearly opaque alpha */
          }
          else {
            v = (uint16_t) ((0x1000 * (plane + 1)) + x + WIDTH * y);
          }
          put_u16_be(&p, v);
        }
      }
    }
    make_box(&idat, "idat", &p, 0, 0);
    free(p.data);
  }

  /* iloc (version 1): item 1 stored in the idat */
  {
    Buf p = {NULL, 0, 0};
    put_u16_be(&p, (4 << 12) | (4 << 8)); /* offset_size=4, length_size=4 */
    put_u16_be(&p, 1);                    /* item_count */
    put_u16_be(&p, 1);                    /* item_ID */
    put_u16_be(&p, 0x0001);               /* construction_method=1 (idat) */
    put_u16_be(&p, 0);                    /* data_reference_index */
    put_u16_be(&p, 1);                    /* extent_count */
    put_u32_be(&p, 0);                    /* extent_offset */
    put_u32_be(&p, WIDTH * HEIGHT * 4 * 2); /* extent_length */
    make_box(&meta, "iloc", &p, 1, 1);
    free(p.data);
  }

  buf_put(&meta, idat.data, idat.size);
  make_box(file, "meta", &meta, 1, 0);
  free(idat.data);
  free(meta.data);
}

int main(void)
{
  Buf file = {NULL, 0, 0};
  build_file(&file);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "heif_context_alloc failed\n");
    return 1;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed: %s\n", err.message);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "get handle failed: %s\n", err.message);
    return 1;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  if (!options || !ext) {
    fprintf(stderr, "options alloc failed\n");
    return 1;
  }

  /* Composite the alpha channel onto a solid background colour. */
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_444, options);
  printf("decode finished: code=%d subcode=%d msg=%s\n", (int) err.code, (int) err.subcode,
         err.code == heif_error_Ok ? "ok" : err.message);

  if (img) {
    printf("output: %ux%u chroma=%d\n", heif_image_get_width(img, heif_channel_R),
           heif_image_get_height(img, heif_channel_R), (int) heif_image_get_chroma_format(img));
    heif_image_release(img);
  }

  options->color_conversion_options_ext = NULL;
  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file.data);
  return 0;
}
