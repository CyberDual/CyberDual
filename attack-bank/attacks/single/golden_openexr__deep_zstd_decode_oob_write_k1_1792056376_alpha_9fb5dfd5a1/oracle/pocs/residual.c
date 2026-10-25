#include <openexr.h>
#include <zstd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
check (exr_result_t rv, const char* what)
{
    if (rv != EXR_ERR_SUCCESS)
    {
        fprintf (stderr, "%s failed: %s\n", what, exr_get_error_code_as_string (rv));
        return 1;
    }
    return 0;
}

int
main (void)
{
    const char*    path     = "/tmp/deep_zstd_residual.exr";
    const uint64_t unpacked = 32;
    const uint64_t split    = unpacked + 8;
    const int32_t  samples  = (int32_t) (split / 2);

    uint8_t inner[8 + 40];
    memset (inner, 0x11, sizeof (inner));
    uint64_t seglen = split;
    memcpy (inner, &seglen, sizeof (seglen));

    size_t   zbound = ZSTD_compressBound (sizeof (inner));
    uint8_t* zpay   = (uint8_t*) malloc (zbound);
    if (!zpay) return 0;
    size_t zsz = ZSTD_compress (zpay, zbound, inner, sizeof (inner), 3);
    if (ZSTD_isError (zsz))
    {
        fprintf (stderr, "zstd compress: %s\n", ZSTD_getErrorName (zsz));
        return 0;
    }
    unsigned long long fcs = ZSTD_getFrameContentSize (zpay, zsz);
    fprintf (stderr, "zstd payload %zu content %llu\n", zsz, fcs);

    const uint8_t magic[8] = {'z', 's', 't', 'd', '-', 'e', 'x', 'r'};
    size_t        framesz  = 24 + zsz;
    uint8_t*      frame    = (uint8_t*) calloc (1, framesz);
    if (!frame) return 0;
    memcpy (frame, magic, 8);
    uint32_t format = 1;
    uint32_t flags  = 0;
    uint64_t psz    = (uint64_t) zsz;
    memcpy (frame + 8, &format, 4);
    memcpy (frame + 12, &flags, 4);
    memcpy (frame + 16, &psz, 8);
    memcpy (frame + 24, zpay, zsz);

    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_context_t             f     = NULL;
    int                       part  = 0;
    if (check (exr_start_write (&f, path, EXR_WRITE_FILE_DIRECTLY, &cinit), "start_write"))
        return 0;
    if (check (exr_add_part (f, "beauty", EXR_STORAGE_DEEP_SCANLINE, &part), "add_part"))
        return 0;
    if (check (
            exr_initialize_required_attr_simple (f, part, 1, 1, EXR_COMPRESSION_ZSTD),
            "attrs"))
        return 0;
    if (check (
            exr_add_channel (
                f, part, "A", EXR_PIXEL_HALF, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1),
            "channel"))
        return 0;
    if (check (exr_write_header (f), "header")) return 0;
    if (check (
            exr_write_deep_scanline_chunk (
                f, part, 0, frame, framesz, unpacked, &samples, sizeof (samples)),
            "write_chunk"))
        return 0;
    if (check (exr_finish (&f), "finish_write")) return 0;

    if (check (exr_start_read (&f, path, &cinit), "start_read")) return 0;
    exr_chunk_info_t cinfo;
    memset (&cinfo, 0, sizeof (cinfo));
    if (check (exr_read_scanline_chunk_info (f, 0, 0, &cinfo), "chunk_info"))
        return 0;
    fprintf (
        stderr,
        "chunk packed %llu unpacked %llu samples %llu\n",
        (unsigned long long) cinfo.packed_size,
        (unsigned long long) cinfo.unpacked_size,
        (unsigned long long) cinfo.sample_count_table_size);

    exr_decode_pipeline_t dec = EXR_DECODE_PIPELINE_INITIALIZER;
    if (check (exr_decoding_initialize (f, 0, &cinfo, &dec), "dec_init")) return 0;

    uint8_t* out = (uint8_t*) calloc (1, 256);
    if (!out) return 0;
    for (int c = 0; c < dec.channel_count; ++c)
    {
        dec.channels[c].decode_to_ptr     = out;
        dec.channels[c].user_pixel_stride = dec.channels[c].user_bytes_per_element;
        dec.channels[c].user_line_stride  = dec.channels[c].user_pixel_stride;
        dec.channels[c].user_bytes_per_element =
            dec.channels[c].bytes_per_element;
    }
    if (check (exr_decoding_choose_default_routines (f, 0, &dec), "choose"))
        return 0;
    exr_result_t rv = exr_decoding_run (f, 0, &dec);
    fprintf (stderr, "decode returned %d\n", (int) rv);
    exr_decoding_destroy (f, &dec);
    exr_finish (&f);
    free (out);
    free (frame);
    free (zpay);
    return 0;
}
