#include <openexr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
err_cb (exr_const_context_t f, int code, const char* msg)
{
    (void) f;
    fprintf (stderr, "exr %d: %s\n", code, msg);
}

enum
{
    IMG_W = 64,
    IMG_H = 32
};

static int
write_image (const char* path)
{
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_encode_pipeline_t     enc;
    exr_chunk_info_t          cinfo;
    int                       part = 0;
    float*                    pixels;

    cinit.error_handler_fn = &err_cb;
    pixels                 = (float*) malloc (sizeof (float) * IMG_W * IMG_H);
    if (!pixels) return 1;
    for (int i = 0; i < IMG_W * IMG_H; ++i)
        pixels[i] = (float) (i + 1);

    if (exr_start_write (&f, path, EXR_WRITE_FILE_DIRECTLY, &cinit))
        return 2;
    if (exr_add_part (f, "scan", EXR_STORAGE_SCANLINE, &part)) return 3;
    if (exr_initialize_required_attr_simple (
            f, part, IMG_W, IMG_H, EXR_COMPRESSION_DWAA))
        return 4;
    if (exr_add_channel (
            f,
            part,
            "A",
            EXR_PIXEL_FLOAT,
            EXR_PERCEPTUALLY_LINEAR,
            1,
            1))
        return 5;
    if (exr_write_header (f)) return 6;

    if (exr_write_scanline_chunk_info (f, 0, 0, &cinfo)) return 7;
    if (exr_encoding_initialize (f, 0, &cinfo, &enc)) return 8;
    for (int c = 0; c < enc.channel_count; ++c)
    {
        enc.channels[c].encode_from_ptr        = (const uint8_t*) pixels;
        enc.channels[c].user_pixel_stride      = (int32_t) sizeof (float);
        enc.channels[c].user_line_stride       = (int32_t) (sizeof (float) * IMG_W);
        enc.channels[c].user_bytes_per_element = (int16_t) sizeof (float);
        enc.channels[c].user_data_type         = EXR_PIXEL_FLOAT;
    }
    if (exr_encoding_choose_default_routines (f, 0, &enc)) return 9;
    if (exr_encoding_run (f, 0, &enc)) return 10;
    exr_encoding_destroy (f, &enc);
    if (exr_finish (&f)) return 11;

    free (pixels);
    return 0;
}

static int
read_image (const char* path)
{
    exr_context_t             f;
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    exr_decode_pipeline_t     dec;
    exr_chunk_info_t          cinfo;
    float*                    out;
    int                       rv;

    cinit.error_handler_fn = &err_cb;
    out                    = (float*) calloc (IMG_W * IMG_H, sizeof (float));
    if (!out) return 1;

    if (exr_start_read (&f, path, &cinit)) return 2;
    if (exr_read_scanline_chunk_info (f, 0, 0, &cinfo)) return 3;
    if (exr_decoding_initialize (f, 0, &cinfo, &dec)) return 4;
    for (int c = 0; c < dec.channel_count; ++c)
    {
        dec.channels[c].decode_to_ptr          = (uint8_t*) out;
        dec.channels[c].user_pixel_stride      = (int32_t) sizeof (float);
        dec.channels[c].user_line_stride       = (int32_t) (sizeof (float) * IMG_W);
        dec.channels[c].user_bytes_per_element = (int16_t) sizeof (float);
        dec.channels[c].user_data_type         = EXR_PIXEL_FLOAT;
    }
    if (exr_decoding_choose_default_routines (f, 0, &dec)) return 5;
    rv = exr_decoding_run (f, 0, &dec);
    exr_decoding_destroy (f, &dec);
    exr_finish (&f);

    if (rv != EXR_ERR_SUCCESS)
    {
        fprintf (stderr, "decode returned %d\n", rv);
        free (out);
        return 6;
    }
    printf ("A0=%g\n", out[0]);
    free (out);
    return 0;
}

int
main (void)
{
    const char* path = "/tmp/dwa_float_a.exr";
    int         rc   = write_image (path);
    if (rc)
    {
        fprintf (stderr, "write failed %d\n", rc);
        return 1;
    }
    rc = read_image (path);
    if (rc)
    {
        fprintf (stderr, "read failed %d\n", rc);
        return 2;
    }
    return 0;
}
