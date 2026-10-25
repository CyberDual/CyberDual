#include <openexr.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void
quiet_err (exr_const_context_t f, int code, const char* msg)
{
    (void) f;
    (void) code;
    (void) msg;
}

static void
rle_store (uint8_t* dst, size_t* n, const uint8_t* raw, size_t len)
{
    size_t i = 0;
    *n       = 0;
    while (i < len)
    {
        size_t c = len - i;
        if (c > 127) c = 127;
        dst[(*n)++] = (uint8_t) (-(int) c);
        memcpy (dst + *n, raw + i, c);
        *n += c;
        i += c;
    }
}

static int
write_alpha (const char* path, int w, int h)
{
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    int                       part  = 0;
    exr_attr_box2i_t          dw;
    exr_chunk_info_t          cinfo;
    exr_encode_pipeline_t     enc;
    uint16_t*                 a;
    int                       i;

    cinit.error_handler_fn = &quiet_err;
    if (exr_start_write (&f, path, EXR_WRITE_FILE_DIRECTLY, &cinit)) return 1;
    if (exr_add_part (f, "scan", EXR_STORAGE_SCANLINE, &part)) return 1;
    if (exr_initialize_required_attr_simple (
            f, part, w, h, EXR_COMPRESSION_DWAA))
        return 1;
    dw.min.x = 0;
    dw.min.y = 0;
    dw.max.x = w - 1;
    dw.max.y = h - 1;
    if (exr_set_data_window (f, part, &dw)) return 1;
    if (exr_add_channel (
            f, part, "A", EXR_PIXEL_HALF, EXR_PERCEPTUALLY_LINEAR, 1, 1))
        return 1;
    if (exr_write_header (f)) return 1;

    a = calloc ((size_t) w * (size_t) h, sizeof (uint16_t));
    if (!a) return 1;
    for (i = 0; i < w * h; ++i)
        a[i] = (uint16_t) (0x3c00 + (i & 0x3f));

    if (exr_write_scanline_chunk_info (f, 0, 0, &cinfo)) return 1;
    if (exr_encoding_initialize (f, 0, &cinfo, &enc)) return 1;
    for (i = 0; i < enc.channel_count; ++i)
    {
        enc.channels[i].encode_from_ptr   = (uint8_t*) a;
        enc.channels[i].user_pixel_stride = (int32_t) sizeof (uint16_t);
        enc.channels[i].user_line_stride  = (int32_t) (sizeof (uint16_t) * (size_t) w);
    }
    if (exr_encoding_choose_default_routines (f, 0, &enc)) return 1;
    if (exr_encoding_run (f, 0, &enc)) return 1;
    exr_encoding_destroy (f, &enc);
    exr_finish (&f);
    free (a);
    return 0;
}

/* Replace the RLE substream with a consistent stream one byte short of the
 * alpha channel's planar size. */
static int
shorten_rle (const char* path, size_t raw_len)
{
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_chunk_info_t          cinfo;
    FILE*                     fp;
    long                      flen;
    uint8_t*                  file = NULL;
    uint8_t*                  raw  = NULL;
    uint8_t*                  rle  = NULL;
    uint8_t*                  zbuf = NULL;
    uint8_t*                  np   = NULL;
    uint64_t                  off;
    uint64_t                  counters[11];
    uint16_t                  rule_sz;
    size_t                    rle_n = 0;
    uLongf                    zlen;
    size_t                    new_pay;
    int32_t                   newsz;
    int                       rc = 1;

    cinit.error_handler_fn = &quiet_err;
    if (exr_start_read (&f, path, &cinit)) return 1;
    if (exr_read_scanline_chunk_info (f, 0, 0, &cinfo)) return 1;
    off = cinfo.data_offset;
    if (cinfo.packed_size == 0 || cinfo.packed_size >= cinfo.unpacked_size)
        return 1;
    exr_finish (&f);

    fp = fopen (path, "rb");
    if (!fp) return 1;
    if (fseek (fp, 0, SEEK_END) != 0) { fclose (fp); return 1; }
    flen = ftell (fp);
    if (flen < 0 || (uint64_t) flen < off + 90) { fclose (fp); return 1; }
    if (fseek (fp, 0, SEEK_SET) != 0) { fclose (fp); return 1; }
    file = malloc ((size_t) flen);
    if (!file || fread (file, 1, (size_t) flen, fp) != (size_t) flen)
    {
        fclose (fp);
        free (file);
        return 1;
    }
    fclose (fp);

    memcpy (counters, file + off, sizeof counters);
    memcpy (&rule_sz, file + off + 88, sizeof rule_sz);
    if (rule_sz < sizeof rule_sz || off + 88 + rule_sz > (uint64_t) flen)
    {
        free (file);
        return 1;
    }

    raw = malloc (raw_len);
    rle = malloc (raw_len * 2 + 8);
    if (!raw || !rle) goto done;
    memset (raw, 0x3c, raw_len);
    rle_store (rle, &rle_n, raw, raw_len);
    zlen = compressBound ((uLong) rle_n);
    zbuf = malloc (zlen);
    if (!zbuf || compress2 (zbuf, &zlen, rle, (uLong) rle_n, 9) != Z_OK)
        goto done;

    counters[1]  = 0;
    counters[2]  = 0;
    counters[3]  = 0;
    counters[4]  = 0;
    counters[5]  = zlen;
    counters[6]  = rle_n;
    counters[7]  = raw_len;
    counters[8]  = 0;
    counters[9]  = 0;
    counters[10] = 0;

    new_pay = 88 + rule_sz + zlen;
    np      = malloc (new_pay);
    if (!np) goto done;
    memcpy (np, counters, 88);
    memcpy (np + 88, file + off + 88, rule_sz);
    memcpy (np + 88 + rule_sz, zbuf, zlen);

    newsz = (int32_t) new_pay;
    fp    = fopen (path, "wb");
    if (!fp) goto done;
    if (fwrite (file, 1, (size_t) off - 4, fp) == (size_t) off - 4 &&
        fwrite (&newsz, 4, 1, fp) == 1 && fwrite (np, 1, new_pay, fp) == new_pay)
        rc = 0;
    fclose (fp);

done:
    free (file);
    free (raw);
    free (rle);
    free (zbuf);
    free (np);
    return rc;
}

static int
read_alpha (const char* path, int w, int h)
{
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_chunk_info_t          cinfo;
    exr_decode_pipeline_t     dec;
    uint16_t*                 a;
    int                       i;
    exr_result_t              rv;

    cinit.error_handler_fn = &quiet_err;
    a = calloc ((size_t) w * (size_t) h, sizeof (uint16_t));
    if (!a) return 1;
    if (exr_start_read (&f, path, &cinit)) { free (a); return 1; }
    if (exr_read_scanline_chunk_info (f, 0, 0, &cinfo))
    {
        exr_finish (&f);
        free (a);
        return 1;
    }
    if (exr_decoding_initialize (f, 0, &cinfo, &dec))
    {
        exr_finish (&f);
        free (a);
        return 1;
    }
    for (i = 0; i < dec.channel_count; ++i)
    {
        dec.channels[i].decode_to_ptr     = (uint8_t*) a;
        dec.channels[i].user_pixel_stride = (int32_t) sizeof (uint16_t);
        dec.channels[i].user_line_stride =
            (int32_t) (sizeof (uint16_t) * (size_t) w);
    }
    if (exr_decoding_choose_default_routines (f, 0, &dec))
    {
        exr_decoding_destroy (f, &dec);
        exr_finish (&f);
        free (a);
        return 1;
    }
    rv = exr_decoding_run (f, 0, &dec);
    exr_decoding_destroy (f, &dec);
    exr_finish (&f);
    free (a);
    return rv == EXR_ERR_SUCCESS ? 0 : 0;
}

int
main (void)
{
    const char* path = "/tmp/dwa_rle_short.exr";
    const int   w    = 64;
    const int   h    = 32;
    size_t      full = (size_t) w * (size_t) h * sizeof (uint16_t);

    if (write_alpha (path, w, h)) return 1;
    if (shorten_rle (path, full - 1)) return 1;
    return read_alpha (path, w, h);
}
