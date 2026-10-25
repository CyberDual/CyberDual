/*
 * Evaluation probe: writes a small lossy LJ2K scanline part that mixes
 * unsubsampled float RGB channels with a subsampled half Z channel, using
 * only the public OpenEXRCore C API.
 *
 * On a tree where the HTJ2K encoder's float-range scan walks the packed
 * buffer over the padded (coded) height instead of the chunk height, the
 * scan reads past the end of the packed buffer and AddressSanitizer
 * aborts inside chunk_max_linear_float() during exr_encoding_run().
 */

#include "openexr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 12
#define H 8
/* Z is subsampled 2x2 */
#define ZW (W / 2)
#define ZH (H / 2)

static float    rgb[3][H][W];
static uint16_t zbuf[ZH][ZW];

static exr_context_t exrfile = NULL;

static void
fill (void)
{
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            rgb[0][y][x] = 0.25f * (float) x + 1.0f;
            rgb[1][y][x] = 0.5f * (float) y + 2.0f;
            rgb[2][y][x] = 4.0f * (float) ((x * y) % 7) + 0.125f;
        }
    for (int y = 0; y < ZH; ++y)
        for (int x = 0; x < ZW; ++x)
            zbuf[y][x] = 0x3c00; /* half(1.0) */
}

#define CHECK(expr)                                                          \
    do                                                                       \
    {                                                                        \
        exr_result_t _r = (expr);                                            \
        if (_r != EXR_ERR_SUCCESS)                                           \
        {                                                                    \
            fprintf (stderr, "failure (%d) at line %d\n", (int) _r, __LINE__); \
            if (exrfile) exr_finish (&exrfile);                              \
            return 1;                                                        \
        }                                                                    \
    } while (0)

int
main (void)
{
    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    int                       part  = -1;
    exr_chunk_info_t          cinfo;
    exr_encode_pipeline_t     encoder;

    fill ();

    CHECK (exr_start_write (
        &exrfile,
        "/tmp/openexr_deloc_probe.exr",
        EXR_WRITE_FILE_DIRECTLY,
        &cinit));
    CHECK (exr_add_part (exrfile, "scan", EXR_STORAGE_SCANLINE, &part));
    CHECK (exr_initialize_required_attr_simple (
        exrfile, part, W, H, EXR_COMPRESSION_LJ2K));
    CHECK (exr_set_lossy_htj2k_quality (exrfile, part, 45.0f));

    CHECK (exr_add_channel (
        exrfile, part, "R", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1));
    CHECK (exr_add_channel (
        exrfile, part, "G", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1));
    CHECK (exr_add_channel (
        exrfile, part, "B", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1));
    CHECK (exr_add_channel (
        exrfile, part, "Z", EXR_PIXEL_HALF, EXR_PERCEPTUALLY_LOGARITHMIC, 2, 2));

    CHECK (exr_write_header (exrfile));

    /* the image is smaller than one 256-line block: a single chunk */
    CHECK (exr_write_scanline_chunk_info (exrfile, part, 0, &cinfo));
    CHECK (exr_encoding_initialize (exrfile, part, &cinfo, &encoder));

    for (int c = 0; c < encoder.channel_count; ++c)
    {
        exr_coding_channel_info_t* ch = &encoder.channels[c];

        if (ch->height == 0) continue;

        if (!strcmp (ch->channel_name, "R"))
        {
            ch->encode_from_ptr   = (const uint8_t*) &rgb[0][0][0];
            ch->user_pixel_stride = 4;
            ch->user_line_stride  = 4 * W;
        }
        else if (!strcmp (ch->channel_name, "G"))
        {
            ch->encode_from_ptr   = (const uint8_t*) &rgb[1][0][0];
            ch->user_pixel_stride = 4;
            ch->user_line_stride  = 4 * W;
        }
        else if (!strcmp (ch->channel_name, "B"))
        {
            ch->encode_from_ptr   = (const uint8_t*) &rgb[2][0][0];
            ch->user_pixel_stride = 4;
            ch->user_line_stride  = 4 * W;
        }
        else
        {
            ch->encode_from_ptr   = (const uint8_t*) &zbuf[0][0];
            ch->user_pixel_stride = 2;
            ch->user_line_stride  = 2 * ZW;
        }
    }

    CHECK (exr_encoding_choose_default_routines (exrfile, part, &encoder));
    CHECK (exr_encoding_run (exrfile, part, &encoder));

    CHECK (exr_encoding_destroy (exrfile, &encoder));
    CHECK (exr_finish (&exrfile));

    printf ("probe completed without error\n");
    return 0;
}
