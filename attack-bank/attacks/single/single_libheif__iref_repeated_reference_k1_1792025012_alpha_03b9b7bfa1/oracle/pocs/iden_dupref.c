/*
 * libheif public-API input.
 *
 * Builds a minimal HEIF file in memory whose 'iref' box contains a single
 * 'dimg' entry that lists the same to_item_ID twice: item 2 is an 'iden'
 * (identity) image, the primary image, that derives from one 8x8 'mski' mask
 * image (item 1), which is listed two times in the 'dimg' reference.
 *
 * The file is written to a temporary file, read back through the public read
 * API (heif_context_read_from_file), and the primary image is decoded.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libheif/heif.h>

static void put8(unsigned char** p, unsigned char v)
{
  *(*p)++ = v;
}

static void put16(unsigned char** p, unsigned int v)
{
  *(*p)++ = (unsigned char) (v >> 8);
  *(*p)++ = (unsigned char) (v & 0xFF);
}

static void put32(unsigned char** p, unsigned int v)
{
  *(*p)++ = (unsigned char) (v >> 24);
  *(*p)++ = (unsigned char) (v >> 16);
  *(*p)++ = (unsigned char) (v >> 8);
  *(*p)++ = (unsigned char) (v & 0xFF);
}

static void put4cc(unsigned char** p, const char* type)
{
  *(*p)++ = (unsigned char) type[0];
  *(*p)++ = (unsigned char) type[1];
  *(*p)++ = (unsigned char) type[2];
  *(*p)++ = (unsigned char) type[3];
}

/* Write a box (optionally a full box) and return the position of its size
 * field, so the size can be patched in once the payload length is known. */
static size_t begin_box(unsigned char* buf, size_t* len, int full_box,
                        const char* type, unsigned char version)
{
  unsigned char* p = buf + *len;
  size_t size_pos = *len;

  put32(&p, 0);
  put4cc(&p, type);
  if (full_box) {
    put8(&p, version);
    put8(&p, 0);
    put8(&p, 0);
    put8(&p, 0);
  }
  *len = (size_t) (p - buf);
  return size_pos;
}

static void end_box(unsigned char* buf, size_t len, size_t size_pos)
{
  unsigned int size = (unsigned int) (len - size_pos);
  buf[size_pos] = (unsigned char) (size >> 24);
  buf[size_pos + 1] = (unsigned char) (size >> 16);
  buf[size_pos + 2] = (unsigned char) (size >> 8);
  buf[size_pos + 3] = (unsigned char) (size & 0xFF);
}

int main(void)
{
  unsigned char buf[4096];
  size_t len = 0;
  unsigned char* p;

  /* 'mski' item 1: 8x8 mask, 8 bits per pixel, stored in 'idat'. */
  unsigned char mask[64];
  memset(mask, 0x7F, sizeof(mask));

  /* --- file type ----------------------------------------------------------- */

  size_t ftyp_pos = begin_box(buf, &len, 0, "ftyp", 0);
  p = buf + len;
  put4cc(&p, "heic");
  put32(&p, 0);
  put4cc(&p, "mif1");
  put4cc(&p, "heic");
  len = (size_t) (p - buf);
  end_box(buf, len, ftyp_pos);

  /* --- meta ---------------------------------------------------------------- */

  size_t meta_pos = begin_box(buf, &len, 1, "meta", 0);

  /* hdlr */
  size_t pos = begin_box(buf, &len, 1, "hdlr", 0);
  p = buf + len;
  put32(&p, 0);
  put4cc(&p, "pict");
  put32(&p, 0);
  put32(&p, 0);
  put32(&p, 0);
  put8(&p, 0);
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  /* pitm: the 'iden' image is the primary image */
  pos = begin_box(buf, &len, 1, "pitm", 0);
  p = buf + len;
  put16(&p, 2);
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  /* iinf: item 1 'mski', item 2 'iden' */
  pos = begin_box(buf, &len, 1, "iinf", 0);
  p = buf + len;
  put16(&p, 2);
  len = (size_t) (p - buf);

  size_t infe1_pos = begin_box(buf, &len, 1, "infe", 2);
  p = buf + len;
  put16(&p, 1);
  put16(&p, 0);
  put4cc(&p, "mski");
  put8(&p, 0);
  len = (size_t) (p - buf);
  end_box(buf, len, infe1_pos);

  size_t infe2_pos = begin_box(buf, &len, 1, "infe", 2);
  p = buf + len;
  put16(&p, 2);
  put16(&p, 0);
  put4cc(&p, "iden");
  put8(&p, 0);
  len = (size_t) (p - buf);
  end_box(buf, len, infe2_pos);

  end_box(buf, len, pos);

  /* iprp / ipco / ipma */
  size_t iprp_pos = begin_box(buf, &len, 0, "iprp", 0);
  size_t ipco_pos = begin_box(buf, &len, 0, "ipco", 0);

  pos = begin_box(buf, &len, 1, "ispe", 0);
  p = buf + len;
  put32(&p, 8);
  put32(&p, 8);
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  pos = begin_box(buf, &len, 1, "mskC", 0);
  p = buf + len;
  put8(&p, 8);   /* bits per pixel */
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  end_box(buf, len, ipco_pos);

  pos = begin_box(buf, &len, 1, "ipma", 0);
  p = buf + len;
  put32(&p, 2);
  put16(&p, 1);             /* item 1 */
  put8(&p, 2);              /* two associations */
  put8(&p, 0x80 | 1);       /* ispe, essential */
  put8(&p, 0x80 | 2);       /* mskC, essential */
  put16(&p, 2);             /* item 2 */
  put8(&p, 1);
  put8(&p, 0x80 | 1);       /* ispe, essential */
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  end_box(buf, len, iprp_pos);

  /* iloc: construction method 1 (data in 'idat'); only the mask has data */
  pos = begin_box(buf, &len, 1, "iloc", 1);
  p = buf + len;
  put8(&p, (4 << 4) | 4);   /* offset_size = 4, length_size = 4 */
  put8(&p, (0 << 4) | 0);  /* base_offset_size = 0, index_size = 0 */
  put16(&p, 1);            /* item_count */
  put16(&p, 1);            /* item 1 */
  put16(&p, 0x0001);       /* construction_method = 1 */
  put16(&p, 0);            /* data_reference_index */
  put16(&p, 1);            /* extent_count */
  put32(&p, 0);            /* extent offset (idat relative) */
  put32(&p, sizeof(mask));
  len = (size_t) (p - buf);
  end_box(buf, len, pos);

  /* iref: one 'dimg' entry from item 2 to item 1, listed twice */
  pos = begin_box(buf, &len, 1, "iref", 0);
  {
    size_t dimg_pos = begin_box(buf, &len, 0, "dimg", 0);
    p = buf + len;
    put16(&p, 2);   /* from_item_ID: the 'iden' image */
    put16(&p, 2);   /* reference_count */
    put16(&p, 1);   /* to_item_ID */
    put16(&p, 1);   /* to_item_ID, repeated */
    len = (size_t) (p - buf);
    end_box(buf, len, dimg_pos);
  }
  end_box(buf, len, pos);

  /* idat */
  pos = begin_box(buf, &len, 0, "idat", 0);
  memcpy(buf + len, mask, sizeof(mask));
  len += sizeof(mask);
  end_box(buf, len, pos);

  end_box(buf, len, meta_pos);

  /* --- write the file and read it back through the public API -------------- */

  const char* path = "/tmp/iden_dupref.heif";
  FILE* f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path);
    return 1;
  }
  fwrite(buf, 1, len, f);
  fclose(f);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    return 1;
  }

  heif_error err = heif_context_read_from_file(ctx, path, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed: %s\n", err.message);
    heif_context_free(ctx);
    return 1;
  }
  fprintf(stderr, "read ok\n");

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "primary handle failed: %s\n", err.message);
    heif_context_free(ctx);
    return 1;
  }

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_undefined,
                          heif_chroma_undefined, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode failed: %s\n", err.message);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return 1;
  }

  fprintf(stderr, "decoded %dx%d\n",
          heif_image_get_width(img, heif_channel_interleaved),
          heif_image_get_height(img, heif_channel_interleaved));

  heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
