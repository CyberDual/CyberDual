#include <libheif/heif.h>
#include <libheif/heif_sequences.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t** p, uint16_t x)
{
  *(*p)++ = (uint8_t)(x >> 8);
  *(*p)++ = (uint8_t)x;
}

static void put32(uint8_t** p, uint32_t x)
{
  *(*p)++ = (uint8_t)(x >> 24);
  *(*p)++ = (uint8_t)(x >> 16);
  *(*p)++ = (uint8_t)(x >> 8);
  *(*p)++ = (uint8_t)x;
}

static void put4cc(uint8_t** p, const char* s)
{
  memcpy(*p, s, 4);
  *p += 4;
}

static uint8_t* box_open(uint8_t* p, const char* type, uint8_t** size_at)
{
  *size_at = p;
  put32(&p, 0);
  put4cc(&p, type);
  return p;
}

static void box_close(uint8_t* p, uint8_t* size_at)
{
  uint32_t n = (uint32_t)(p - size_at);
  size_at[0] = (uint8_t)(n >> 24);
  size_at[1] = (uint8_t)(n >> 16);
  size_at[2] = (uint8_t)(n >> 8);
  size_at[3] = (uint8_t)n;
}

/* Minimal 'uncv' image sequence: component-interleaved 8-bit 4:2:2. */
static uint8_t* build_file(size_t* out_size)
{
  const uint16_t width = 128;
  const uint16_t height = 128;
  const uint32_t y_bytes = (uint32_t)width * height;
  const uint32_t c_bytes = (uint32_t)(width / 2) * height;
  const uint32_t sample_size = y_bytes + c_bytes + c_bytes;

  uint8_t* buf = (uint8_t*)calloc(1, 4096u + sample_size);
  if (!buf) {
    return NULL;
  }
  uint8_t* p = buf;
  uint8_t* sz;

  p = box_open(p, "ftyp", &sz);
  put4cc(&p, "msf1");
  put32(&p, 0);
  put4cc(&p, "msf1");
  put4cc(&p, "isom");
  box_close(p, sz);

  p = box_open(p, "moov", &sz);
  uint8_t* moov_sz = sz;

  p = box_open(p, "mvhd", &sz);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 1000);
  put32(&p, 1);
  put32(&p, 0x00010000);
  put16(&p, 0x0100);
  put16(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  for (int i = 0; i < 9; i++) put32(&p, 0);
  for (int i = 0; i < 6; i++) put32(&p, 0);
  put32(&p, 2);
  box_close(p, sz);

  p = box_open(p, "trak", &sz);
  uint8_t* trak_sz = sz;

  p = box_open(p, "tkhd", &sz);
  put32(&p, 0x00000003);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 1);
  put32(&p, 0);
  put32(&p, 1);
  put32(&p, 0);
  put32(&p, 0);
  put16(&p, 0);
  put16(&p, 0);
  put16(&p, 0);
  put16(&p, 0);
  for (int i = 0; i < 9; i++) put32(&p, 0);
  put32(&p, ((uint32_t)width) << 16);
  put32(&p, ((uint32_t)height) << 16);
  box_close(p, sz);

  p = box_open(p, "mdia", &sz);
  uint8_t* mdia_sz = sz;

  p = box_open(p, "mdhd", &sz);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 1000);
  put32(&p, 1);
  put16(&p, 0x55c4);
  put16(&p, 0);
  box_close(p, sz);

  p = box_open(p, "hdlr", &sz);
  put32(&p, 0);
  put32(&p, 0);
  put4cc(&p, "pict");
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  *p++ = 0;
  box_close(p, sz);

  p = box_open(p, "minf", &sz);
  uint8_t* minf_sz = sz;

  p = box_open(p, "stbl", &sz);
  uint8_t* stbl_sz = sz;

  p = box_open(p, "stsd", &sz);
  uint8_t* stsd_sz = sz;
  put32(&p, 0);
  put32(&p, 1);

  p = box_open(p, "uncv", &sz);
  uint8_t* uncv_sz = sz;
  for (int i = 0; i < 6; i++) *p++ = 0;
  put16(&p, 1);
  put16(&p, 0);
  put16(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put16(&p, width);
  put16(&p, height);
  put32(&p, 0x00480000);
  put32(&p, 0x00480000);
  put32(&p, 0);
  put16(&p, 1);
  for (int i = 0; i < 32; i++) *p++ = 0;
  put16(&p, 0x0018);
  put16(&p, 0xFFFF);

  p = box_open(p, "cmpd", &sz);
  put32(&p, 3);
  put16(&p, 1); /* Y */
  put16(&p, 2); /* Cb */
  put16(&p, 3); /* Cr */
  box_close(p, sz);

  p = box_open(p, "uncC", &sz);
  put32(&p, 0);          /* version 0 */
  put32(&p, 0);          /* profile */
  put32(&p, 3);
  for (uint16_t idx = 0; idx < 3; idx++) {
    put16(&p, idx);
    *p++ = 7;            /* 8-bit stored as depth-1 */
    *p++ = 0;            /* unsigned */
    *p++ = 0;
  }
  *p++ = 1;              /* 4:2:2 */
  *p++ = 0;              /* component interleave */
  *p++ = 0;
  *p++ = 0;
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);          /* one tile column */
  put32(&p, 0);          /* one tile row */
  box_close(p, sz);

  box_close(p, uncv_sz);
  box_close(p, stsd_sz);

  p = box_open(p, "stts", &sz);
  put32(&p, 0);
  put32(&p, 1);
  put32(&p, 1);
  put32(&p, 1);
  box_close(p, sz);

  p = box_open(p, "stsc", &sz);
  put32(&p, 0);
  put32(&p, 1);
  put32(&p, 1);
  put32(&p, 1);
  put32(&p, 1);
  box_close(p, sz);

  p = box_open(p, "stsz", &sz);
  put32(&p, 0);
  put32(&p, sample_size);
  put32(&p, 1);
  box_close(p, sz);

  p = box_open(p, "stco", &sz);
  put32(&p, 0);
  put32(&p, 1);
  uint8_t* chunk_off = p;
  put32(&p, 0);
  box_close(p, sz);

  box_close(p, stbl_sz);
  box_close(p, minf_sz);
  box_close(p, mdia_sz);
  box_close(p, trak_sz);
  box_close(p, moov_sz);

  uint32_t mdat_payload = (uint32_t)(p - buf) + 8;
  chunk_off[0] = (uint8_t)(mdat_payload >> 24);
  chunk_off[1] = (uint8_t)(mdat_payload >> 16);
  chunk_off[2] = (uint8_t)(mdat_payload >> 8);
  chunk_off[3] = (uint8_t)mdat_payload;

  p = box_open(p, "mdat", &sz);
  memset(p, 0x80, sample_size);
  p += sample_size;
  box_close(p, sz);

  *out_size = (size_t)(p - buf);
  return buf;
}

int main(void)
{
  size_t size = 0;
  uint8_t* file = build_file(&size);
  if (!file) {
    return 1;
  }

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_memory(ctx, file, size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed: %s\n", err.message ? err.message : "");
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  heif_track* track = heif_context_get_track(ctx, 0);
  if (!track) {
    fprintf(stderr, "no visual track\n");
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  heif_decoding_options* opts = heif_decoding_options_alloc();
  opts->output_image_nclx_profile_passthrough = 1;

  heif_image* img = NULL;
  err = heif_track_decode_next_image(track, &img,
                                     heif_colorspace_YCbCr, heif_chroma_422,
                                     opts);
  heif_decoding_options_free(opts);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode failed: %s\n", err.message ? err.message : "");
    heif_track_release(track);
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  int stride = 0;
  const uint8_t* y = heif_image_get_plane_readonly(img, heif_channel_Y, &stride);
  if (!y || heif_image_get_primary_width(img) != 128 ||
      heif_image_get_primary_height(img) != 128) {
    fprintf(stderr, "unexpected frame\n");
    heif_image_release(img);
    heif_track_release(track);
    heif_context_free(ctx);
    free(file);
    return 1;
  }

  heif_image_release(img);
  heif_track_release(track);
  heif_context_free(ctx);
  free(file);
  return 0;
}
