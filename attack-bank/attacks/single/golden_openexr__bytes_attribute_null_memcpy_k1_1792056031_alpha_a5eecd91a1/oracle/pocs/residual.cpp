// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.

// Open a single-part scanline EXR whose header contains a zero-length
// "bytes" attribute with an empty type hint.

#include "ImfMultiPartInputFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{

void
append (std::vector<unsigned char>& out, const void* data, size_t n)
{
    const auto* p = static_cast<const unsigned char*> (data);
    out.insert (out.end (), p, p + n);
}

void
appendU32 (std::vector<unsigned char>& out, uint32_t v)
{
    unsigned char b[4] = {
        static_cast<unsigned char> (v & 0xff),
        static_cast<unsigned char> ((v >> 8) & 0xff),
        static_cast<unsigned char> ((v >> 16) & 0xff),
        static_cast<unsigned char> ((v >> 24) & 0xff),
    };
    append (out, b, 4);
}

void
appendI32 (std::vector<unsigned char>& out, int32_t v)
{
    appendU32 (out, static_cast<uint32_t> (v));
}

void
appendStr (std::vector<unsigned char>& out, const char* s)
{
    append (out, s, std::strlen (s) + 1);
}

void
attr (
    std::vector<unsigned char>& out,
    const char*                 name,
    const char*                 type,
    const std::vector<unsigned char>& value)
{
    appendStr (out, name);
    appendStr (out, type);
    appendI32 (out, static_cast<int32_t> (value.size ()));
    append (out, value.data (), value.size ());
}

std::vector<unsigned char>
minimalExr ()
{
    std::vector<unsigned char> ch;
    appendStr (ch, "R");
    appendI32 (ch, 1); // HALF
    unsigned char flags[4] = {0, 0, 0, 0};
    append (ch, flags, 4);
    appendI32 (ch, 1);
    appendI32 (ch, 1);
    ch.push_back (0); // end of channel list

    std::vector<unsigned char> box;
    appendI32 (box, 0);
    appendI32 (box, 0);
    appendI32 (box, 0);
    appendI32 (box, 0);

    std::vector<unsigned char> f32 (4, 0);
    // 1.0f little-endian
    f32[0] = 0x00;
    f32[1] = 0x00;
    f32[2] = 0x80;
    f32[3] = 0x3f;
    std::vector<unsigned char> v2f (8, 0);

    // bytes attribute value: uint32 hint length 0, no hint text, no payload
    std::vector<unsigned char> bytesVal;
    appendU32 (bytesVal, 0);

    std::vector<unsigned char> file;
    appendU32 (file, 20000630u); // magic
    appendU32 (file, 2u);        // version 2, single-part scanline

    attr (file, "channels", "chlist", ch);
    attr (file, "compression", "compression", std::vector<unsigned char> (1, 0));
    attr (file, "dataWindow", "box2i", box);
    attr (file, "displayWindow", "box2i", box);
    attr (file, "lineOrder", "lineOrder", std::vector<unsigned char> (1, 0));
    attr (file, "pixelAspectRatio", "float", f32);
    attr (file, "screenWindowCenter", "v2f", v2f);
    attr (file, "screenWindowWidth", "float", f32);
    attr (file, "emptybytes", "bytes", bytesVal);
    file.push_back (0); // end of header

    const uint64_t scanPos = static_cast<uint64_t> (file.size ()) + 8;
    unsigned char off[8];
    for (int i = 0; i < 8; ++i)
        off[i] = static_cast<unsigned char> ((scanPos >> (8 * i)) & 0xff);
    append (file, off, 8);

    appendI32 (file, 0); // y
    appendI32 (file, 2); // packed sample size
    unsigned char px[2] = {0, 0};
    append (file, px, 2);
    return file;
}

} // namespace

int
main ()
{
    const char*                  path = "/tmp/empty_bytes_attr.exr";
    const std::vector<unsigned char> exr  = minimalExr ();
    {
        std::ofstream os (path, std::ios::binary);
        if (!os)
            return 2;
        os.write (
            reinterpret_cast<const char*> (exr.data ()),
            static_cast<std::streamsize> (exr.size ()));
    }

    OPENEXR_IMF_NAMESPACE::MultiPartInputFile in (path);
    (void) in.header (0);
    return 0;
}
