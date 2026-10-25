#include <ImfArray.h>
#include <ImfChannelList.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfDeepScanLineInputFile.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfHeader.h>
#include <ImfPartType.h>

#include <ImathBox.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static bool
patch_sample_count (const char* path)
{
    std::ifstream in (path, std::ios::binary);
    if (!in) return false;
    std::vector<unsigned char> b (
        (std::istreambuf_iterator<char> (in)),
        std::istreambuf_iterator<char> ());
    in.close ();
    if (b.size () < 8) return false;

    size_t i = 8;
    while (i < b.size ())
    {
        size_t n0 = i;
        while (n0 < b.size () && b[n0] != 0) ++n0;
        if (n0 >= b.size ()) return false;
        bool end = (n0 == i);
        i        = n0 + 1;
        if (end) break;
        size_t t0 = i;
        while (t0 < b.size () && b[t0] != 0) ++t0;
        if (t0 + 4 >= b.size ()) return false;
        i = t0 + 1;
        int32_t sz;
        std::memcpy (&sz, &b[i], 4);
        i += 4;
        if (sz < 0 || i + (size_t) sz > b.size ()) return false;
        i += (size_t) sz;
    }
    if (i + 8 > b.size ()) return false;
    uint64_t off;
    std::memcpy (&off, &b[i], 8);
    if (off + 4 + 24 > b.size ()) return false;
    size_t c = (size_t) off + 4;
    int64_t scsz, psz, usz;
    std::memcpy (&scsz, &b[c], 8);
    std::memcpy (&psz, &b[c + 8], 8);
    std::memcpy (&usz, &b[c + 16], 8);
    c += 24;
    if (scsz != 4 || usz <= 4) return false;
    if (c + 4 > b.size ()) return false;
    int32_t one = 1;
    std::memcpy (&b[c], &one, 4);

    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write ((const char*) b.data (), (std::streamsize) b.size ());
    return out.good ();
}

static void
write_deep (const char* path)
{
    const int samples = 64;
    Header    hdr (
        Box2i (V2i (0, 0), V2i (0, 0)),
        Box2i (V2i (0, 0), V2i (0, 0)),
        1,
        V2f (0, 0),
        1,
        INCREASING_Y,
        ZSTD_COMPRESSION);
    hdr.channels ().insert ("Z", Channel (FLOAT));
    hdr.setType (DEEPSCANLINE);

    Array2D<unsigned int> sc (1, 1);
    Array2D<float*>       data (1, 1);
    sc[0][0]   = (unsigned) samples;
    data[0][0] = new float[samples];
    for (int s = 0; s < samples; ++s)
        data[0][0][s] = 0.25f * (float) (s + 1);

    {
        DeepScanLineOutputFile file (path, hdr, 1);
        DeepFrameBuffer        fb;
        fb.insertSampleCountSlice (Slice (
            UINT,
            (char*) (&sc[0][0]),
            sizeof (unsigned int),
            sizeof (unsigned int)));
        fb.insert (
            "Z",
            DeepSlice (
                FLOAT,
                (char*) (&data[0][0]),
                sizeof (float*),
                sizeof (float*),
                sizeof (float)));
        file.setFrameBuffer (fb);
        file.writePixels (1);
    }
    delete[] data[0][0];
}

static int
read_deep (const char* path)
{
    DeepScanLineInputFile in (path, 1);
    Box2i                 dw = in.header ().dataWindow ();
    int                   w  = dw.max.x - dw.min.x + 1;
    int                   h  = dw.max.y - dw.min.y + 1;
    Array2D<unsigned int> sc (h, w);
    Array2D<float*>       data (h, w);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            data[y][x] = new float[64];

    int             memOff = dw.min.x + dw.min.y * w;
    DeepFrameBuffer fb;
    fb.insertSampleCountSlice (Slice (
        UINT,
        (char*) (&sc[0][0] - memOff),
        sizeof (unsigned int),
        sizeof (unsigned int) * w));
    fb.insert (
        "Z",
        DeepSlice (
            FLOAT,
            (char*) (&data[0][0] - memOff),
            sizeof (float*),
            sizeof (float*) * w,
            sizeof (float)));
    in.setFrameBuffer (fb);
    in.readPixels (dw.min.y, dw.max.y);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            delete[] data[y][x];
    return 0;
}

int
main ()
{
    const char* path = "/tmp/exr_deep_zstd_short_grid.exr";
    try
    {
        write_deep (path);
        if (!patch_sample_count (path))
        {
            std::fprintf (stderr, "could not adjust sample count\n");
            return 2;
        }
        read_deep (path);
    }
    catch (const std::exception& e)
    {
        std::fprintf (stderr, "exception: %s\n", e.what ());
        return 2;
    }
    std::printf ("read completed\n");
    return 0;
}
