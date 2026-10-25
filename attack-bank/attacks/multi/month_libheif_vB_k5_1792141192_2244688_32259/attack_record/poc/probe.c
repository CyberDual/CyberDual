// Probe: decode a YCbCr 4:2:0 'unci' image with Y=16 bit and Cb=Cr=8 bit to a
// 16-bit interleaved RGB target through the libheif public API.
//
// The file is structurally valid: a single 'unci' item (mixed interleave,
// 4:2:0 sampling, per-component bit depths as ISO/IEC 23001-17 allows) stored
// in 'idat'. On a correct library the conversion to the 16-bit interleaved
// target is refused cleanly (heif_error_Unsupported_feature) because no
// conversion can widen the 8-bit chroma planes to 16 bits.

#include <libheif/heif.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 256u
#define HEIGHT 256u
#define CHROMA_WIDTH (WIDTH / 2)
#define CHROMA_HEIGHT (HEIGHT / 2)

typedef struct {
  uint8_t* data;
  size_t size;
  size_t cap;
} buf;

static void buf_put(buf* b, const void* p, size_t n)
{
  if (b->size + n > b->cap) {
    size_t ncap = (b->cap ? b->cap * 2 : 4096);
    while (ncap < b->size + n) ncap *= 2;
    b->data = (uint8_t*) realloc(b->data, ncap);
    if (!b->data) { fprintf(stderr, "oom\n"); exit(2); }
    b->cap = ncap;
  }
  memcpy(b->data + b->size, p, n);
  b->size += n;
}

static void put_u8(buf* b, uint8_t v) { buf_put(b, &v, 1); }

static void put_u16_be(buf* b, uint16_t v)
{
  uint8_t t[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xff)};
  buf_put(b, t, 2);
}

static void put_u32_be(buf* b, uint32_t v)
{
  uint8_t t[4] = {(uint8_t)(v >> 24), (uint8_t)((v >> 16) & 0xff),
                  (uint8_t)((v >> 8) & 0xff), (uint8_t)(v & 0xff)};
  buf_put(b, t, 4);
}

static void put_fourcc(buf* b, const char cc[4]) { buf_put(b, cc, 4); }

// Append a box (size+type header followed by payload).
static void put_box(buf* b, const char type[4], const buf* payload)
{
  put_u32_be(b, (uint32_t)(8 + payload->size));
  put_fourcc(b, type);
  buf_put(b, payload->data, payload->size);
}

// Append a full box (version/flags word between header and payload).
static void put_full_box(buf* b, const char type[4], uint8_t version, uint32_t flags, const buf* payload)
{
  buf p = {0};
  put_u8(&p, version);
  put_u8(&p, (uint8_t)((flags >> 16) & 0xff));
  put_u8(&p, (uint8_t)((flags >> 8) & 0xff));
  put_u8(&p, (uint8_t)(flags & 0xff));
  buf_put(&p, payload->data, payload->size);
  put_box(b, type, &p);
  free(p.data);
}

static void build_heif_unci_ycbcr_mismatched_luma_depth(buf* file)
{
  // ftyp
  {
    buf p = {0};
    put_fourcc(&p, "mif1");
    put_u32_be(&p, 0);
    put_fourcc(&p, "mif1");
    put_fourcc(&p, "heic");
    put_box(file, "ftyp", &p);
    free(p.data);
  }

  buf meta = {0};

  // hdlr
  {
    buf p = {0};
    put_u32_be(&p, 0);             // pre_defined
    put_fourcc(&p, "pict");        // handler_type
    put_u32_be(&p, 0);
    put_u32_be(&p, 0);
    put_u32_be(&p, 0);
    put_u8(&p, 0);                 // name
    put_full_box(&meta, "hdlr", 0, 0, &p);
    free(p.data);
  }

  // pitm: primary item 1
  {
    buf p = {0};
    put_u16_be(&p, 1);
    put_full_box(&meta, "pitm", 0, 0, &p);
    free(p.data);
  }

  // iinf with one infe (version 2): item 1 = 'unci'
  {
    buf infe = {0};
    put_u16_be(&infe, 1);          // item_ID
    put_u16_be(&infe, 0);          // item_protection_index
    put_fourcc(&infe, "unci");     // item_type
    put_u8(&infe, 0);              // item_name
    buf infe_box = {0};
    put_full_box(&infe_box, "infe", 2, 0, &infe);
    free(infe.data);

    buf p = {0};
    put_u16_be(&p, 1);             // entry_count
    buf_put(&p, infe_box.data, infe_box.size);
    free(infe_box.data);
    put_full_box(&meta, "iinf", 0, 0, &p);
    free(p.data);
  }

  // iprp: ipco (ispe, cmpd, uncC, colr) + ipma
  {
    buf ipco = {0};

    // ispe
    {
      buf p = {0};
      put_u32_be(&p, WIDTH);
      put_u32_be(&p, HEIGHT);
      put_full_box(&ipco, "ispe", 0, 0, &p);
      free(p.data);
    }

    // cmpd: Y, Cb, Cr
    {
      buf p = {0};
      put_u32_be(&p, 3);
      put_u16_be(&p, 1); // Y
      put_u16_be(&p, 2); // Cb
      put_u16_be(&p, 3); // Cr
      put_box(&ipco, "cmpd", &p);
      free(p.data);
    }

    // uncC (version 0): mixed interleave, 4:2:0, Y=16 bit, Cb=8 bit, Cr=8 bit
    {
      static const uint8_t depths[3] = {16, 8, 8}; // Y, Cb, Cr
      buf p = {0};
      put_u32_be(&p, 0);           // profile
      put_u32_be(&p, 3);           // component_count
      for (uint16_t idx = 0; idx < 3; idx++) {
        put_u16_be(&p, idx);                       // component_index
        put_u8(&p, (uint8_t)(depths[idx] - 1));    // component_bit_depth_minus_one
        put_u8(&p, 0);                             // component_format (unsigned)
        put_u8(&p, 0);                             // component_align_size
      }
      put_u8(&p, 2);               // sampling_type = 4:2:0
      put_u8(&p, 2);               // interleave_type = mixed
      put_u8(&p, 0);               // block_size
      put_u8(&p, 0);               // flags (big-endian components)
      put_u32_be(&p, 0);           // pixel_size
      put_u32_be(&p, 0);           // row_align_size
      put_u32_be(&p, 0);           // tile_align_size
      put_u32_be(&p, 0);           // num_tile_cols_minus_one
      put_u32_be(&p, 0);           // num_tile_rows_minus_one
      put_full_box(&ipco, "uncC", 0, 0, &p);
      free(p.data);
    }

    // colr (nclx): BT.601 matrix so the YCbCr->RGB operators are candidates
    {
      buf p = {0};
      put_fourcc(&p, "nclx");
      put_u16_be(&p, 6);   // colour_primaries (smpte170m)
      put_u16_be(&p, 6);   // transfer_characteristics (smpte170m)
      put_u16_be(&p, 6);   // matrix_coefficients (BT.601)
      put_u8(&p, 0x80);    // full_range_flag = 1
      put_box(&ipco, "colr", &p);
      free(p.data);
    }

    // ipma: item 1 <- ispe(essential), cmpd(essential), uncC(essential), colr
    buf ipma = {0};
    put_u32_be(&ipma, 1);          // entry_count
    put_u16_be(&ipma, 1);          // item_ID
    put_u8(&ipma, 4);              // association_count
    put_u8(&ipma, 0x80 | 1);       // essential, ispe
    put_u8(&ipma, 0x80 | 2);       // essential, cmpd
    put_u8(&ipma, 0x80 | 3);       // essential, uncC
    put_u8(&ipma, 4);              // non-essential, colr

    buf iprp = {0};
    put_box(&iprp, "ipco", &ipco);
    {
      buf ipma_full = {0};
      put_full_box(&ipma_full, "ipma", 0, 0, &ipma);
      buf_put(&iprp, ipma_full.data, ipma_full.size);
      free(ipma_full.data);
    }
    free(ipco.data);
    free(ipma.data);

    put_box(&meta, "iprp", &iprp);
    free(iprp.data);
  }

  // iloc (version 1): item 1 stored in idat (construction_method=1)
  {
    buf p = {0};
    put_u16_be(&p, (4 << 12) | (4 << 8)); // offset_size=4, length_size=4, base_offset_size=0, index_size=0
    put_u16_be(&p, 1);      // item_count
    put_u16_be(&p, 1);      // item_ID
    put_u16_be(&p, 0x0001); // construction_method=1 (idat)
    put_u16_be(&p, 0);      // data_reference_index
    put_u16_be(&p, 1);      // extent_count
    put_u32_be(&p, 0);      // extent_offset (within idat)
    // extent_length: Y (2 bytes/sample) + interleaved Cb/Cr (1 byte each)
    put_u32_be(&p, WIDTH * HEIGHT * 2 + CHROMA_WIDTH * CHROMA_HEIGHT * 2);
    put_full_box(&meta, "iloc", 1, 0, &p);
    free(p.data);
  }

  // idat: Y plane (row-major, 2 bytes/sample, big-endian), then the
  // interleaved chroma block (Cb, Cr; 1 byte each per chroma position).
  {
    buf p = {0};
    for (uint32_t y = 0; y < HEIGHT; y++) {
      for (uint32_t x = 0; x < WIDTH; x++) {
        put_u16_be(&p, (uint16_t)(0x1000 + x + WIDTH * y));
      }
    }
    for (uint32_t y = 0; y < CHROMA_HEIGHT; y++) {
      for (uint32_t x = 0; x < CHROMA_WIDTH; x++) {
        put_u8(&p, (uint8_t)(0x40 + y * CHROMA_WIDTH + x)); // Cb
        put_u8(&p, (uint8_t)(0x80 + y * CHROMA_WIDTH + x)); // Cr
      }
    }
    put_box(&meta, "idat", &p);
    free(p.data);
  }

  put_full_box(file, "meta", 0, 0, &meta);
  free(meta.data);
}

// Ask the AddressSanitizer runtime to abort (SIGABRT) once it has printed a
// report, so a memory-safety fault surfaces as an abort instead of a plain
// error exit.
const char* __asan_default_options(void)
{
  return "abort_on_error=1";
}

int main(void)
{
  buf file = {0};
  build_heif_unci_ycbcr_mismatched_luma_depth(&file);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "heif_context_alloc failed\n");
    return 2;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed: %s\n", err.message);
    heif_context_free(ctx);
    free(file.data);
    return 2;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || !handle) {
    fprintf(stderr, "no primary image: %s\n", err.message);
    heif_context_free(ctx);
    free(file.data);
    return 2;
  }

  // Ask for the 16-bit interleaved RGB target. A correct library refuses this
  // conversion cleanly for an image whose chroma planes are shallower than the
  // luma plane.
  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBB_LE, NULL);
  if (err.code != heif_error_Ok) {
    printf("decode refused cleanly: %s\n", err.message);
    if (img) heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    free(file.data);
    return 0;
  }

  printf("decoded %dx%d\n", heif_image_get_width(img, heif_channel_interleaved),
         heif_image_get_height(img, heif_channel_interleaved));

  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  free(file.data);
  return 0;
}
