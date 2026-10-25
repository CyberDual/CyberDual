#include "libheif/heif.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 256U
#define HEIGHT 256U

typedef struct {
  uint8_t* data;
  size_t size;
  size_t capacity;
} Buffer;

static void buffer_reserve(Buffer* buffer, size_t extra) {
  if (buffer->size + extra <= buffer->capacity) {
    return;
  }

  size_t capacity = buffer->capacity ? buffer->capacity : 256;
  while (capacity < buffer->size + extra) {
    capacity *= 2;
  }

  uint8_t* data = realloc(buffer->data, capacity);
  if (!data) {
    fprintf(stderr, "out of memory\n");
    exit(2);
  }

  buffer->data = data;
  buffer->capacity = capacity;
}

static void append(Buffer* buffer, const void* data, size_t size) {
  buffer_reserve(buffer, size);
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
}

static void append_u8(Buffer* buffer, uint8_t value) {
  append(buffer, &value, 1);
}

static void append_u16(Buffer* buffer, uint16_t value) {
  const uint8_t bytes[2] = {
      (uint8_t)(value >> 8),
      (uint8_t)value
  };
  append(buffer, bytes, sizeof(bytes));
}

static void append_u32(Buffer* buffer, uint32_t value) {
  const uint8_t bytes[4] = {
      (uint8_t)(value >> 24),
      (uint8_t)(value >> 16),
      (uint8_t)(value >> 8),
      (uint8_t)value
  };
  append(buffer, bytes, sizeof(bytes));
}

static void append_fourcc(Buffer* buffer, const char value[4]) {
  append(buffer, value, 4);
}

static void append_box(Buffer* output, const char type[4], const Buffer* payload) {
  append_u32(output, (uint32_t)(8 + payload->size));
  append_fourcc(output, type);
  append(output, payload->data, payload->size);
}

static void append_full_box(Buffer* output, const char type[4], uint8_t version,
                            uint32_t flags, const Buffer* payload) {
  Buffer box = {0};
  append_u8(&box, version);
  append_u8(&box, (uint8_t)((flags >> 16) & 0xffU));
  append_u8(&box, (uint8_t)((flags >> 8) & 0xffU));
  append_u8(&box, (uint8_t)(flags & 0xffU));
  append(&box, payload->data, payload->size);
  append_box(output, type, &box);
  free(box.data);
}

static void append_box_buffer(Buffer* output, const char type[4], const Buffer* payload) {
  append_box(output, type, payload);
}

static Buffer build_file(void) {
  Buffer file = {0};
  Buffer meta = {0};

  Buffer ftyp_payload = {0};
  append_fourcc(&ftyp_payload, "mif1");
  append_u32(&ftyp_payload, 0);
  append_fourcc(&ftyp_payload, "mif1");
  append_fourcc(&ftyp_payload, "heic");
  Buffer ftyp = {0};
  append_box_buffer(&ftyp, "ftyp", &ftyp_payload);
  append(&file, ftyp.data, ftyp.size);

  Buffer hdlr_payload = {0};
  append_u32(&hdlr_payload, 0);
  append_fourcc(&hdlr_payload, "pict");
  append_u32(&hdlr_payload, 0);
  append_u32(&hdlr_payload, 0);
  append_u32(&hdlr_payload, 0);
  append_u8(&hdlr_payload, 0);
  append_full_box(&meta, "hdlr", 0, 0, &hdlr_payload);

  Buffer pitm_payload = {0};
  append_u16(&pitm_payload, 1);
  append_full_box(&meta, "pitm", 0, 0, &pitm_payload);

  Buffer infe_payload = {0};
  append_u16(&infe_payload, 1);
  append_u16(&infe_payload, 0);
  append_fourcc(&infe_payload, "unci");
  append_u8(&infe_payload, 0);
  Buffer infe = {0};
  append_full_box(&infe, "infe", 2, 0, &infe_payload);

  Buffer iinf_payload = {0};
  append_u16(&iinf_payload, 1);
  append(&iinf_payload, infe.data, infe.size);
  append_full_box(&meta, "iinf", 0, 0, &iinf_payload);

  Buffer ispe_payload = {0};
  append_u32(&ispe_payload, WIDTH);
  append_u32(&ispe_payload, HEIGHT);
  Buffer ispe = {0};
  append_full_box(&ispe, "ispe", 0, 0, &ispe_payload);

  Buffer cmpd_payload = {0};
  append_u32(&cmpd_payload, 2);
  append_u16(&cmpd_payload, 0); /* monochrome */
  append_u16(&cmpd_payload, 7); /* alpha */
  Buffer cmpd = {0};
  append_box_buffer(&cmpd, "cmpd", &cmpd_payload);

  Buffer uncC_payload = {0};
  append_u32(&uncC_payload, 0);
  append_u32(&uncC_payload, 2);
  append_u16(&uncC_payload, 0);
  append_u8(&uncC_payload, 15); /* 16-bit luma */
  append_u8(&uncC_payload, 0);
  append_u8(&uncC_payload, 0);
  append_u16(&uncC_payload, 1);
  append_u8(&uncC_payload, 7);  /* 8-bit alpha */
  append_u8(&uncC_payload, 0);
  append_u8(&uncC_payload, 0);
  append_u8(&uncC_payload, 0);  /* no subsampling */
  append_u8(&uncC_payload, 0);  /* component interleave */
  append_u8(&uncC_payload, 0);
  append_u8(&uncC_payload, 0);
  append_u32(&uncC_payload, 0);
  append_u32(&uncC_payload, 0);
  append_u32(&uncC_payload, 0);
  append_u32(&uncC_payload, 0);
  append_u32(&uncC_payload, 0);
  Buffer uncC = {0};
  append_full_box(&uncC, "uncC", 0, 0, &uncC_payload);

  Buffer ipco_payload = {0};
  append(&ipco_payload, ispe.data, ispe.size);
  append(&ipco_payload, cmpd.data, cmpd.size);
  append(&ipco_payload, uncC.data, uncC.size);
  Buffer ipco = {0};
  append_box_buffer(&ipco, "ipco", &ipco_payload);

  Buffer ipma_payload = {0};
  append_u32(&ipma_payload, 1);
  append_u16(&ipma_payload, 1);
  append_u8(&ipma_payload, 3);
  append_u8(&ipma_payload, 0x80 | 1);
  append_u8(&ipma_payload, 0x80 | 2);
  append_u8(&ipma_payload, 0x80 | 3);
  Buffer ipma = {0};
  append_full_box(&ipma, "ipma", 0, 0, &ipma_payload);

  Buffer iprp_payload = {0};
  append(&iprp_payload, ipco.data, ipco.size);
  append(&iprp_payload, ipma.data, ipma.size);
  Buffer iprp = {0};
  append_box_buffer(&iprp, "iprp", &iprp_payload);
  append(&meta, iprp.data, iprp.size);

  Buffer tile_data = {0};
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      append_u16(&tile_data, (uint16_t)(0x1000 + x + WIDTH * y));
    }
  }
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      append_u8(&tile_data, (uint8_t)(x + y));
    }
  }
  Buffer idat = {0};
  append_box_buffer(&idat, "idat", &tile_data);

  Buffer iloc_payload = {0};
  append_u16(&iloc_payload, (4 << 12) | (4 << 8));
  append_u16(&iloc_payload, 1);
  append_u16(&iloc_payload, 1);
  append_u16(&iloc_payload, 0x0001);
  append_u16(&iloc_payload, 0);
  append_u16(&iloc_payload, 1);
  append_u32(&iloc_payload, 0);
  append_u32(&iloc_payload, (uint32_t)tile_data.size);
  append_full_box(&meta, "iloc", 1, 0, &iloc_payload);
  append(&meta, idat.data, idat.size);

  Buffer meta_box = {0};
  append_full_box(&meta_box, "meta", 0, 0, &meta);
  append(&file, meta_box.data, meta_box.size);

  free(ftyp_payload.data); free(ftyp.data);
  free(hdlr_payload.data); free(pitm_payload.data);
  free(infe_payload.data); free(infe.data); free(iinf_payload.data);
  free(ispe_payload.data); free(ispe.data);
  free(cmpd_payload.data); free(cmpd.data); free(uncC_payload.data); free(uncC.data);
  free(ipco_payload.data); free(ipco.data); free(ipma_payload.data); free(ipma.data);
  free(iprp_payload.data); free(iprp.data);
  free(tile_data.data); free(idat.data); free(iloc_payload.data);
  free(meta.data); free(meta_box.data);

  return file;
}

int main(void) {
  Buffer file = build_file();
  heif_context* context = heif_context_alloc();
  if (!context) {
    return 2;
  }

  heif_error error = heif_context_read_from_memory_without_copy(
      context, file.data, (size_t)file.size, NULL);
  if (error.code != heif_error_Ok) {
    fprintf(stderr, "read: %s\n", error.message);
    return 2;
  }

  heif_image_handle* handle = NULL;
  error = heif_context_get_primary_image_handle(context, &handle);
  if (error.code != heif_error_Ok || !handle) {
    fprintf(stderr, "handle: %s\n", error.message);
    return 2;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* conversion =
      heif_color_conversion_options_ext_alloc();
  if (!options || !conversion) {
    return 2;
  }
  conversion->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  conversion->background_red = 0xffff;
  conversion->background_green = 0xffff;
  conversion->background_blue = 0xffff;
  options->color_conversion_options_ext = conversion;

  heif_image* image = NULL;
  error = heif_decode_image(handle, &image, heif_colorspace_monochrome,
                            heif_chroma_monochrome, options);
  fprintf(stderr, "decode: %s\n", error.message);

  if (image) {
    heif_image_release(image);
  }
  heif_color_conversion_options_ext_free(conversion);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(context);
  free(file.data);
  return error.code == heif_error_Ok ? 0 : 1;
}
