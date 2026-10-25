// Decodes a 16-bit planar RGBA image (uncompressed 'unci' codec) to planar RGB
// with alpha compositing onto a solid background, through the public libheif API.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libheif/heif.h"

static void put_u16_be(uint8_t** p, uint16_t x) {
  *(*p)++ = (uint8_t)(x >> 8);
  *(*p)++ = (uint8_t)(x & 0xff);
}
static void put_u32_be(uint8_t** p, uint32_t x) {
  *(*p)++ = (uint8_t)(x >> 24);
  *(*p)++ = (uint8_t)(x >> 16);
  *(*p)++ = (uint8_t)(x >> 8);
  *(*p)++ = (uint8_t)(x & 0xff);
}
static void put_fourcc(uint8_t** p, const char* cc) {
  memcpy(*p, cc, 4);
  *p += 4;
}

typedef struct {
  uint8_t* data;
  size_t size;
  size_t cap;
} buf;

static void buf_reserve(buf* b, size_t n) {
  if (b->size + n > b->cap) {
    b->cap = (b->size + n) * 2 + 1024;
    b->data = (uint8_t*)realloc(b->data, b->cap);
  }
}
static void buf_put(buf* b, const uint8_t* d, size_t n) {
  buf_reserve(b, n);
  memcpy(b->data + b->size, d, n);
  b->size += n;
}
static void buf_u16(buf* b, uint16_t x) { uint8_t t[2]; uint8_t* p = t; put_u16_be(&p, x); buf_put(b, t, 2); }
static void buf_u32(buf* b, uint32_t x) { uint8_t t[4]; uint8_t* p = t; put_u32_be(&p, x); buf_put(b, t, 4); }
static void buf_fourcc(buf* b, const char* cc) { buf_put(b, (const uint8_t*)cc, 4); }
static void buf_u8(buf* b, uint8_t x) { buf_put(b, &x, 1); }

static void make_box(buf* out, const char* type, const buf* payload, int full, uint8_t version) {
  buf_u32(out, (uint32_t)(payload->size + 8 + (full ? 4 : 0)));
  buf_fourcc(out, type);
  if (full) {
    buf_u8(out, version);
    buf_u8(out, 0);
    buf_u8(out, 0);
    buf_u8(out, 0);
  }
  buf_put(out, payload->data, payload->size);
}

// Minimal HEIF: one 'unci' item, planar (component interleave), 4:4:4,
// R/G/B/Alpha each 16 bit, big-endian samples.
static void build_heif(buf* file, uint32_t width, uint32_t height) {
  const int depths[4] = {16, 16, 16, 16};

  buf ftyp = {0};
  buf_fourcc(&ftyp, "mif1");
  buf_u32(&ftyp, 0);
  buf_fourcc(&ftyp, "mif1");
  buf_fourcc(&ftyp, "heic");
  buf ftyp_box = {0};
  make_box(&ftyp_box, "ftyp", &ftyp, 0, 0);

  buf hdlr = {0};
  buf_u32(&hdlr, 0);
  buf_fourcc(&hdlr, "pict");
  buf_u32(&hdlr, 0);
  buf_u32(&hdlr, 0);
  buf_u32(&hdlr, 0);
  buf_u8(&hdlr, 0);
  buf hdlr_box = {0};
  make_box(&hdlr_box, "hdlr", &hdlr, 1, 0);

  buf pitm = {0};
  buf_u16(&pitm, 1);
  buf pitm_box = {0};
  make_box(&pitm_box, "pitm", &pitm, 1, 0);

  buf infe = {0};
  buf_u16(&infe, 1);
  buf_u16(&infe, 0);
  buf_fourcc(&infe, "unci");
  buf_u8(&infe, 0);
  buf infe_box = {0};
  make_box(&infe_box, "infe", &infe, 1, 2);

  buf iinf = {0};
  buf_u16(&iinf, 1);
  buf_put(&iinf, infe_box.data, infe_box.size);
  buf iinf_box = {0};
  make_box(&iinf_box, "iinf", &iinf, 1, 0);

  buf ispe = {0};
  buf_u32(&ispe, width);
  buf_u32(&ispe, height);
  buf ispe_box = {0};
  make_box(&ispe_box, "ispe", &ispe, 1, 0);

  buf cmpd = {0};
  buf_u32(&cmpd, 4);
  buf_u16(&cmpd, 4); // red
  buf_u16(&cmpd, 5); // green
  buf_u16(&cmpd, 6); // blue
  buf_u16(&cmpd, 7); // alpha
  buf cmpd_box = {0};
  make_box(&cmpd_box, "cmpd", &cmpd, 0, 0);

  buf uncC = {0};
  buf_u32(&uncC, 0); // profile
  buf_u32(&uncC, 4); // component_count
  for (uint16_t i = 0; i < 4; i++) {
    buf_u16(&uncC, i);
    buf_u8(&uncC, (uint8_t)(depths[i] - 1));
    buf_u8(&uncC, 0);
    buf_u8(&uncC, 0);
  }
  buf_u8(&uncC, 0); // sampling 4:4:4
  buf_u8(&uncC, 0); // interleave component (planar)
  buf_u8(&uncC, 0); // block_size
  buf_u8(&uncC, 0); // big-endian
  buf_u32(&uncC, 0);
  buf_u32(&uncC, 0);
  buf_u32(&uncC, 0);
  buf_u32(&uncC, 0);
  buf_u32(&uncC, 0);
  buf uncC_box = {0};
  make_box(&uncC_box, "uncC", &uncC, 1, 0);

  buf ipco = {0};
  buf_put(&ipco, ispe_box.data, ispe_box.size);
  buf_put(&ipco, cmpd_box.data, cmpd_box.size);
  buf_put(&ipco, uncC_box.data, uncC_box.size);
  buf ipco_box = {0};
  make_box(&ipco_box, "ipco", &ipco, 0, 0);

  buf ipma = {0};
  buf_u32(&ipma, 1);
  buf_u16(&ipma, 1);
  buf_u8(&ipma, 3);
  buf_u8(&ipma, 0x80 | 1);
  buf_u8(&ipma, 0x80 | 2);
  buf_u8(&ipma, 0x80 | 3);
  buf ipma_box = {0};
  make_box(&ipma_box, "ipma", &ipma, 1, 0);

  buf iprp = {0};
  buf_put(&iprp, ipco_box.data, ipco_box.size);
  buf_put(&iprp, ipma_box.data, ipma_box.size);
  buf iprp_box = {0};
  make_box(&iprp_box, "iprp", &iprp, 0, 0);

  buf tile = {0};
  for (int c = 0; c < 4; c++) {
    for (uint32_t i = 0; i < width * height; i++) {
      uint16_t sample = (c == 3) ? (uint16_t)0xFF00 : (uint16_t)(0x0800 + 17 * ((i + 31 * c) & 0x3ff));
      buf_u16(&tile, sample);
    }
  }
  buf idat_box = {0};
  make_box(&idat_box, "idat", &tile, 0, 0);

  buf iloc = {0};
  buf_u16(&iloc, (4 << 12) | (4 << 8));
  buf_u16(&iloc, 1);
  buf_u16(&iloc, 1);
  buf_u16(&iloc, 0x0001); // construction_method = idat
  buf_u16(&iloc, 0);
  buf_u16(&iloc, 1);
  buf_u32(&iloc, 0);
  buf_u32(&iloc, (uint32_t)tile.size);
  buf iloc_box = {0};
  make_box(&iloc_box, "iloc", &iloc, 1, 1);

  buf meta = {0};
  buf_put(&meta, hdlr_box.data, hdlr_box.size);
  buf_put(&meta, pitm_box.data, pitm_box.size);
  buf_put(&meta, iinf_box.data, iinf_box.size);
  buf_put(&meta, iprp_box.data, iprp_box.size);
  buf_put(&meta, iloc_box.data, iloc_box.size);
  buf_put(&meta, idat_box.data, idat_box.size);
  buf meta_box = {0};
  make_box(&meta_box, "meta", &meta, 1, 0);

  buf_put(file, ftyp_box.data, ftyp_box.size);
  buf_put(file, meta_box.data, meta_box.size);

  free(ftyp.data); free(ftyp_box.data); free(hdlr.data); free(hdlr_box.data);
  free(pitm.data); free(pitm_box.data); free(infe.data); free(infe_box.data);
  free(iinf.data); free(iinf_box.data); free(ispe.data); free(ispe_box.data);
  free(cmpd.data); free(cmpd_box.data); free(uncC.data); free(uncC_box.data);
  free(ipco.data); free(ipco_box.data); free(ipma.data); free(ipma_box.data);
  free(iprp.data); free(iprp_box.data); free(tile.data); free(idat_box.data);
  free(iloc.data); free(iloc_box.data); free(meta.data); free(meta_box.data);
}

int main(void) {
  const uint32_t width = 64, height = 64;

  buf file = {0};
  build_heif(&file, width, height);

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read: %s\n", err.message);
    return 2;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "handle: %s\n", err.message);
    return 2;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0x8000;
  ext->background_blue = 0x0000;
  options->color_conversion_options_ext = ext;

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_444, options);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode: %s\n", err.message);
    return 1;
  }

  printf("decoded %dx%d, alpha channel present: %d\n",
         heif_image_get_width(img, heif_channel_R),
         heif_image_get_height(img, heif_channel_R),
         heif_image_has_channel(img, heif_channel_Alpha));

  heif_image_release(img);
  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file.data);
  return 0;
}
