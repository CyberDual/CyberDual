#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64u
#define HEIGHT 64u

typedef struct {
  uint8_t* data;
  size_t size;
  size_t capacity;
} Buffer;

static void buffer_reserve(Buffer* buffer, size_t additional)
{
  size_t required = buffer->size + additional;
  if (required <= buffer->capacity) {
    return;
  }

  size_t capacity = buffer->capacity ? buffer->capacity : 128;
  while (capacity < required) {
    capacity *= 2;
  }

  uint8_t* data = realloc(buffer->data, capacity);
  if (data == NULL) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  buffer->data = data;
  buffer->capacity = capacity;
}

static void buffer_put(Buffer* buffer, const void* data, size_t size)
{
  buffer_reserve(buffer, size);
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
}

static void put8(Buffer* buffer, uint8_t value)
{
  buffer_put(buffer, &value, sizeof(value));
}

static void put16(Buffer* buffer, uint16_t value)
{
  uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
  buffer_put(buffer, bytes, sizeof(bytes));
}

static void put32(Buffer* buffer, uint32_t value)
{
  uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
                      (uint8_t)(value >> 8), (uint8_t)value};
  buffer_put(buffer, bytes, sizeof(bytes));
}

static void put_fourcc(Buffer* buffer, const char* value)
{
  buffer_put(buffer, value, 4);
}

static void buffer_append(Buffer* buffer, const Buffer* other)
{
  buffer_put(buffer, other->data, other->size);
}

static Buffer make_box(const char* type, const Buffer* payload, int full,
                       uint8_t version, uint32_t flags)
{
  Buffer box = {0};
  put32(&box, (uint32_t)(8 + payload->size + (full ? 4 : 0)));
  put_fourcc(&box, type);
  if (full) {
    put32(&box, ((uint32_t)version << 24) | flags);
  }
  buffer_append(&box, payload);
  return box;
}

static void buffer_free(Buffer* buffer)
{
  free(buffer->data);
  buffer->data = NULL;
  buffer->size = 0;
  buffer->capacity = 0;
}

static Buffer build_file(void)
{
  Buffer ftyp_payload = {0};
  put_fourcc(&ftyp_payload, "mif1");
  put32(&ftyp_payload, 0);
  put_fourcc(&ftyp_payload, "mif1");
  put_fourcc(&ftyp_payload, "heic");
  Buffer ftyp = make_box("ftyp", &ftyp_payload, 0, 0, 0);

  Buffer hdlr_payload = {0};
  put32(&hdlr_payload, 0);
  put_fourcc(&hdlr_payload, "pict");
  put32(&hdlr_payload, 0);
  put32(&hdlr_payload, 0);
  put32(&hdlr_payload, 0);
  put8(&hdlr_payload, 0);
  Buffer hdlr = make_box("hdlr", &hdlr_payload, 1, 0, 0);

  Buffer pitm_payload = {0};
  put16(&pitm_payload, 1);
  Buffer pitm = make_box("pitm", &pitm_payload, 1, 0, 0);

  Buffer infe_payload = {0};
  put16(&infe_payload, 1);
  put16(&infe_payload, 0);
  put_fourcc(&infe_payload, "unci");
  put8(&infe_payload, 0);
  Buffer infe = make_box("infe", &infe_payload, 1, 2, 0);

  Buffer iinf_payload = {0};
  put16(&iinf_payload, 1);
  buffer_append(&iinf_payload, &infe);
  Buffer iinf = make_box("iinf", &iinf_payload, 1, 0, 0);

  Buffer ispe_payload = {0};
  put32(&ispe_payload, WIDTH);
  put32(&ispe_payload, HEIGHT);
  Buffer ispe = make_box("ispe", &ispe_payload, 1, 0, 0);

  Buffer cmpd_payload = {0};
  put32(&cmpd_payload, 2);
  put16(&cmpd_payload, 0);
  put16(&cmpd_payload, 7);
  Buffer cmpd = make_box("cmpd", &cmpd_payload, 0, 0, 0);

  Buffer unc_payload = {0};
  put32(&unc_payload, 0);
  put32(&unc_payload, 2);
  for (uint16_t component = 0; component < 2; component++) {
    put16(&unc_payload, component);
    put8(&unc_payload, component == 1 ? 7 : 15);
    put8(&unc_payload, 0);
    put8(&unc_payload, 0);
  }
  put8(&unc_payload, 0);
  put8(&unc_payload, 0);
  put8(&unc_payload, 0);
  put8(&unc_payload, 0);
  put32(&unc_payload, 0);
  put32(&unc_payload, 0);
  put32(&unc_payload, 0);
  put32(&unc_payload, 0);
  put32(&unc_payload, 0);
  Buffer unc = make_box("uncC", &unc_payload, 1, 0, 0);

  Buffer ipco_payload = {0};
  buffer_append(&ipco_payload, &ispe);
  buffer_append(&ipco_payload, &cmpd);
  buffer_append(&ipco_payload, &unc);
  Buffer ipco = make_box("ipco", &ipco_payload, 0, 0, 0);

  Buffer ipma_payload = {0};
  put32(&ipma_payload, 1);
  put16(&ipma_payload, 1);
  put8(&ipma_payload, 3);
  put8(&ipma_payload, 0x81);
  put8(&ipma_payload, 0x82);
  put8(&ipma_payload, 0x83);
  Buffer ipma = make_box("ipma", &ipma_payload, 1, 0, 0);

  Buffer iprp_payload = {0};
  buffer_append(&iprp_payload, &ipco);
  buffer_append(&iprp_payload, &ipma);
  Buffer iprp = make_box("iprp", &iprp_payload, 0, 0, 0);

  Buffer tile = {0};
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      put16(&tile, (uint16_t)(0x1000 + x + WIDTH * y));
    }
  }
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      put8(&tile, 0xff);
    }
  }
  Buffer idat = make_box("idat", &tile, 0, 0, 0);

  Buffer iloc_payload = {0};
  put16(&iloc_payload, (uint16_t)((4u << 12) | (4u << 8)));
  put16(&iloc_payload, 1);
  put16(&iloc_payload, 1);
  put16(&iloc_payload, 0x0001);
  put16(&iloc_payload, 0);
  put16(&iloc_payload, 1);
  put32(&iloc_payload, 0);
  put32(&iloc_payload, (uint32_t)tile.size);
  Buffer iloc = make_box("iloc", &iloc_payload, 1, 1, 0);

  Buffer meta_payload = {0};
  buffer_append(&meta_payload, &hdlr);
  buffer_append(&meta_payload, &pitm);
  buffer_append(&meta_payload, &iinf);
  buffer_append(&meta_payload, &iprp);
  buffer_append(&meta_payload, &iloc);
  buffer_append(&meta_payload, &idat);
  Buffer meta = make_box("meta", &meta_payload, 1, 0, 0);

  Buffer file = {0};
  buffer_append(&file, &ftyp);
  buffer_append(&file, &meta);

  buffer_free(&ftyp_payload);
  buffer_free(&ftyp);
  buffer_free(&hdlr_payload);
  buffer_free(&hdlr);
  buffer_free(&pitm_payload);
  buffer_free(&pitm);
  buffer_free(&infe_payload);
  buffer_free(&infe);
  buffer_free(&iinf_payload);
  buffer_free(&iinf);
  buffer_free(&ispe_payload);
  buffer_free(&ispe);
  buffer_free(&cmpd_payload);
  buffer_free(&cmpd);
  buffer_free(&unc_payload);
  buffer_free(&unc);
  buffer_free(&ipco_payload);
  buffer_free(&ipco);
  buffer_free(&ipma_payload);
  buffer_free(&ipma);
  buffer_free(&iprp_payload);
  buffer_free(&iprp);
  buffer_free(&tile);
  buffer_free(&idat);
  buffer_free(&iloc_payload);
  buffer_free(&iloc);
  buffer_free(&meta_payload);
  buffer_free(&meta);

  return file;
}

static void fail(const char* message, heif_error error)
{
  fprintf(stderr, "%s: %s\n", message, error.message ? error.message : "");
  exit(1);
}

int main(void)
{
  Buffer file = build_file();
  heif_context* context = heif_context_alloc();
  if (context == NULL) {
    return 1;
  }

  heif_error error = heif_context_read_from_memory_without_copy(
      context, file.data, file.size, NULL);
  if (error.code != heif_error_Ok) {
    fail("read", error);
  }

  heif_image_handle* handle = NULL;
  error = heif_context_get_primary_image_handle(context, &handle);
  if (error.code != heif_error_Ok) {
    fail("handle", error);
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* conversion =
      heif_color_conversion_options_ext_alloc();
  if (options == NULL || conversion == NULL) {
    return 1;
  }

  conversion->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  conversion->background_red = 0xffff;
  conversion->background_green = 0xffff;
  conversion->background_blue = 0xffff;
  options->color_conversion_options_ext = conversion;

  heif_image* image = NULL;
  error = heif_decode_image(handle, &image, heif_colorspace_monochrome,
                            heif_chroma_monochrome, options);
  if (error.code != heif_error_Ok) {
    fail("decode", error);
  }

  heif_image_release(image);
  heif_color_conversion_options_ext_free(conversion);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(context);
  buffer_free(&file);
  return 0;
}
