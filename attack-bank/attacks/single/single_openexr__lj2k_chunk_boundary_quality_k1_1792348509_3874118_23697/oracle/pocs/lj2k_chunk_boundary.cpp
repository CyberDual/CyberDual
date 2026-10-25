// Crafted-input probe for the LJ2K chunk decoder.
//
// Writes a small single-chunk LJ2K scanline image, then grows the data
// window of the written file by one scanline.  The chunk is then decoded
// with one more row than its codestream codes, which must be rejected (or
// handled) by the reader without writing outside the chunk's decode
// buffer.
//
// build: c++ -std=c++14 -g -O1 -fsanitize=address <this file> \
//            -I<build dir> -I<src>/src/lib/OpenEXRCore -lOpenEXR ... \
//        run: ASAN_OPTIONS=abort_on_error=1 ./a.out
//
// expected: library rejects the chunk (exit code 1) -- an AddressSanitizer
// report means the decoder stored more rows than the chunk holds.

#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

#include <Imath/half.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static const int W = 96;
static const int H = 100;

static void
writeImage (const char* path, const std::vector<half>& r,
            const std::vector<half>& g, const std::vector<half>& b)
{
    Header hdr (
        Box2i (V2i (0, 0), V2i (W - 1, H - 1)),
        Box2i (V2i (0, 0), V2i (W - 1, H - 1)));
    hdr.compression () = LJ2K_COMPRESSION;
    hdr.channels ().insert ("R", Channel (HALF));
    hdr.channels ().insert ("G", Channel (HALF));
    hdr.channels ().insert ("B", Channel (HALF));

    FrameBuffer fb;
    fb.insert ("R", Slice (HALF, (char*) r.data (), sizeof (half), sizeof (half) * W));
    fb.insert ("G", Slice (HALF, (char*) g.data (), sizeof (half), sizeof (half) * W));
    fb.insert ("B", Slice (HALF, (char*) b.data (), sizeof (half), sizeof (half) * W));

    OutputFile out (path, hdr);
    out.setFrameBuffer (fb);
    out.writePixels (H);
}

/* grow the data window of a written file by one scanline: the attribute
   "dataWindow" of type "box2i" holds four little-endian int32 values,
   the last of which is yMax */
static void
extendDataWindow (const char* path)
{
    std::ifstream in (path, std::ios::binary);
    std::vector<char> bytes ((std::istreambuf_iterator<char> (in)),
                             std::istreambuf_iterator<char> ());
    in.close ();
    if (bytes.size () < 64) throw std::runtime_error ("short file");

    static const char attr[] = "dataWindow\0"
                               "box2i\0";
    void* found             = memmem (bytes.data (), bytes.size (), attr, sizeof (attr) - 1);
    if (!found) throw std::runtime_error ("dataWindow attribute not found");

    char*  ymax = (char*) found + (sizeof (attr) - 1) + 4 + 12;
    int32_t cur;
    memcpy (&cur, ymax, 4);
    if (cur != H - 1) throw std::runtime_error ("unexpected dataWindow");

    cur = H;
    memcpy (ymax, &cur, 4);

    std::ofstream of (path, std::ios::binary | std::ios::trunc);
    of.write (bytes.data (), bytes.size ());
}

int
main (int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "/tmp/lj2k_poc.exr";

    std::vector<half> r (W * H), g (W * H), b (W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            double u = x / (double) W, v = y / (double) H;
            r[y * W + x] = (half) (0.5 + 0.5 * sin (2 * M_PI * (3.1 * u + 1.7 * v)));
            g[y * W + x] = (half) (0.5 + 0.5 * cos (2 * M_PI * (2.3 * u + 2.9 * v)));
            b[y * W + x] = (half) (0.5 + 0.5 * sin (2 * M_PI * (1.1 * u + 4.1 * v)));
        }

    remove (path);

    std::vector<half> out (W * (H + 1) * 3);
    try
    {
        writeImage (path, r, g, b);
        extendDataWindow (path);

        InputFile in (path);
        FrameBuffer fb;
        fb.insert ("R", Slice (HALF, (char*) (out.data () + 0 * W * (H + 1)), sizeof (half), sizeof (half) * W));
        fb.insert ("G", Slice (HALF, (char*) (out.data () + 1 * W * (H + 1)), sizeof (half), sizeof (half) * W));
        fb.insert ("B", Slice (HALF, (char*) (out.data () + 2 * W * (H + 1)), sizeof (half), sizeof (half) * W));
        in.setFrameBuffer (fb);
        in.readPixels (0, H);
    }
    catch (const std::exception& e)
    {
        printf ("library rejected the crafted chunk: %s\n", e.what ());
        return 1;
    }

    printf ("decoded without error\n");
    return 0;
}
