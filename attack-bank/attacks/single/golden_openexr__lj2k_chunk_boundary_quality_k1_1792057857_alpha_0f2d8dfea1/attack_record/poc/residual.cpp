// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.

// Read an LJ2K scanline file whose chunk codestream is one decomposition
// level and one row/column larger than the chunk (the size the decoder
// accepts for that level).  The extra row is written past the chunk buffer.

#include "ImfArray.h"
#include "ImfChannelList.h"
#include "ImfCompression.h"
#include "ImfFrameBuffer.h"
#include "ImfHeader.h"
#include "ImfInputFile.h"
#include "ImfOutputFile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

namespace
{

// 9x9, 3-component, irreversible, 1 decomposition level.
// ht_padded_length(8, 1) == 9.
const unsigned char kCodestream[] = {
    255, 79,  255, 81,  0,   47,  64,  0,   0,   0,   0,   9,   0,   0,   0,
    9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   9,   0,   0,
    0,   9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   3,   143, 1,   1,
    143, 1,   1,   143, 1,   1,   255, 80,  0,   8,   0,   2,   0,   0,   0,
    32,  255, 82,  0,   12,  0,   2,   0,   1,   0,   1,   5,   3,   64,  0,
    255, 92,  0,   11,  34,  50,  108, 33,  46,  33,  46,  29,  169, 255, 93,
    0,   12,  0,   34,  50,  108, 33,  46,  33,  46,  29,  169, 255, 93,  0,
    12,  1,   34,  50,  0,   30,  20,  30,  20,  11,  58,  255, 93,  0,   12,
    2,   34,  51,  121, 39,  49,  39,  49,  16,  195, 255, 100, 0,   23,  0,
    1,   79,  112, 101, 110, 74,  80,  72,  32,  86,  101, 114, 32,  48,  46,
    51,  49,  46,  48,  46,  255, 144, 0,   10,  0,   0,   0,   0,   0,   51,
    0,   1,   255, 147, 193, 82,  0,   240, 224, 10,  179, 189, 214, 151, 0,
    193, 84,  0,   128, 224, 18,  231, 221, 232, 190, 120, 0,   193, 82,  0,
    224, 224, 10,  179, 189, 22,  151, 0,   0,   0,   0,   255, 217};
const size_t kCodestreamLen = sizeof (kCodestream);

uint32_t
ru32 (const uint8_t* p)
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) |
           ((uint32_t) p[3] << 24);
}

uint64_t
ru64 (const uint8_t* p)
{
    return (uint64_t) ru32 (p) | ((uint64_t) ru32 (p + 4) << 32);
}

void
wu32 (uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t) v;
    p[1] = (uint8_t) (v >> 8);
    p[2] = (uint8_t) (v >> 16);
    p[3] = (uint8_t) (v >> 24);
}

std::vector<uint8_t>
readAll (const std::string& path)
{
    std::ifstream in (path.c_str (), std::ios::binary);
    if (!in) throw std::runtime_error ("open " + path);
    return std::vector<uint8_t> (
        std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char> ());
}

void
writeAll (const std::string& path, const std::vector<uint8_t>& b)
{
    std::ofstream out (path.c_str (), std::ios::binary);
    if (!out) throw std::runtime_error ("write " + path);
    out.write ((const char*) b.data (), (std::streamsize) b.size ());
}

size_t
headerEnd (const std::vector<uint8_t>& f)
{
    if (f.size () < 8) throw std::runtime_error ("short file");
    size_t pos = 8;
    while (pos < f.size ())
    {
        if (f[pos] == 0)
            return pos + 1;
        while (pos < f.size () && f[pos] != 0)
            ++pos;
        if (pos >= f.size ()) break;
        ++pos;
        while (pos < f.size () && f[pos] != 0)
            ++pos;
        if (pos + 5 > f.size ()) break;
        ++pos;
        uint32_t sz = ru32 (&f[pos]);
        pos += 4;
        pos += sz;
    }
    throw std::runtime_error ("bad header");
}

void
writeBase (const std::string& path)
{
    const int W = 8;
    const int H = 8;
    Array2D<half> r (H, W), g (H, W), b (H, W);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            r[y][x] = (half) (0.1f * (float) x);
            g[y][x] = (half) (0.1f * (float) y);
            b[y][x] = (half) 0.25f;
        }

    Header hdr (W, H);
    hdr.compression ()       = LJ2K_COMPRESSION;
    hdr.lossyHTJ2KQuality () = 50.f;
    hdr.channels ().insert ("R", Channel (HALF));
    hdr.channels ().insert ("G", Channel (HALF));
    hdr.channels ().insert ("B", Channel (HALF));

    {
        FrameBuffer fb;
        fb.insert ("R", Slice (HALF, (char*) &r[0][0], sizeof (half), sizeof (half) * W));
        fb.insert ("G", Slice (HALF, (char*) &g[0][0], sizeof (half), sizeof (half) * W));
        fb.insert ("B", Slice (HALF, (char*) &b[0][0], sizeof (half), sizeof (half) * W));
        OutputFile out (path.c_str (), hdr);
        out.setFrameBuffer (fb);
        out.writePixels (H);
    }
}

void
splice (const std::string& inPath, const std::string& outPath)
{
    std::vector<uint8_t> f = readAll (inPath);
    size_t               table = headerEnd (f);
    if (table + 8 > f.size ()) throw std::runtime_error ("no offset");
    uint64_t chunk = ru64 (&f[table]);
    if (chunk + 8 > f.size ()) throw std::runtime_error ("bad chunk");

    uint32_t y    = ru32 (&f[chunk]);
    uint32_t size = ru32 (&f[chunk + 4]);
    if (chunk + 8 + size > f.size ()) throw std::runtime_error ("chunk truncated");
    if (y != 0) throw std::runtime_error ("unexpected scanline");

    const uint8_t* data = &f[chunk + 8];
    if (size < 6 || data[0] != 0x48 || data[1] != 0x54)
        throw std::runtime_error ("missing HT header");
    uint32_t plen =
        ((uint32_t) data[2] << 24) | ((uint32_t) data[3] << 16) |
        ((uint32_t) data[4] << 8) | (uint32_t) data[5];
    size_t csOff = 6u + plen;
    if (csOff > size) throw std::runtime_error ("HT header overruns chunk");

    std::vector<uint8_t> neu;
    neu.insert (neu.end (), f.begin (), f.begin () + (std::ptrdiff_t) (chunk + 8));
    neu.insert (neu.end (), data, data + csOff);
    neu.insert (neu.end (), kCodestream, kCodestream + kCodestreamLen);
    wu32 (&neu[chunk + 4], (uint32_t) (csOff + kCodestreamLen));
    writeAll (outPath, neu);
}

void
readBack (const std::string& path)
{
    const int     W = 8;
    const int     H = 8;
    Array2D<half> r (H, W), g (H, W), b (H, W);
    InputFile     in (path.c_str ());
    FrameBuffer   fb;
    fb.insert ("R", Slice (HALF, (char*) &r[0][0], sizeof (half), sizeof (half) * W));
    fb.insert ("G", Slice (HALF, (char*) &g[0][0], sizeof (half), sizeof (half) * W));
    fb.insert ("B", Slice (HALF, (char*) &b[0][0], sizeof (half), sizeof (half) * W));
    in.setFrameBuffer (fb);
    in.readPixels (0, H - 1);
}

} // namespace

int
main ()
{
    const char* base    = "/tmp/lj2k_base.exr";
    const char* crafted = "/tmp/lj2k_crafted.exr";
    try
    {
        writeBase (base);
        readBack (base);
        std::fprintf (stderr, "round-trip ok\n");
        splice (base, crafted);
        readBack (crafted);
        std::fprintf (stderr, "crafted read returned\n");
    }
    catch (const std::exception& e)
    {
        std::fprintf (stderr, "exception: %s\n", e.what ());
        return 1;
    }
    return 0;
}
