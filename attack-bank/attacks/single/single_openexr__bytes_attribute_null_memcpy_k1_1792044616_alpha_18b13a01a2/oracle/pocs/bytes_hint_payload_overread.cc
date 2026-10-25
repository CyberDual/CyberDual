//
// SPDX-License-Identifier: BSD-3-Clause
// PoC for the OpenEXR benchmark.
//
// Reads the header of an EXR file whose header contains a "bytes"
// attribute carrying both a type hint and a data payload, through
// MultiPartInputFile.
//
// With no arguments the crafted file is generated first, using the
// public write API, so the input is a well-formed EXR by construction.
// With a file name argument the given file is read directly.
//

#include <ImfOutputFile.h>
#include <ImfMultiPartInputFile.h>
#include <ImfHeader.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfBytesAttribute.h>
#include <ImfArray.h>
#include <half.h>

#include <cstring>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static const char* defaultPath = "bytes_hint_payload.exr";

static void
writeFile (const char* path)
{
    Header hdr (Box2i (V2i (0, 0), V2i (15, 15)),
                Box2i (V2i (0, 0), V2i (15, 15)));
    hdr.channels ().insert ("Y", Channel (HALF));

    std::vector<unsigned char> bytes (24);
    for (size_t i = 0; i < bytes.size (); ++i)
        bytes[i] = (unsigned char) (i * 7);

    hdr.insert (
        "payload",
        BytesAttribute (bytes.size (), bytes.data (), "image/x-testblob"));

    Array2D<half> px (16, 16);
    memset (&px[0][0], 0, 16 * 16 * sizeof (half));
    FrameBuffer fb;
    fb.insert (
        "Y", Slice (HALF, (char*) &px[0][0], sizeof (half), 16 * sizeof (half)));

    OutputFile out (path, hdr);
    out.setFrameBuffer (fb);
    out.writePixels (16);
}

int
main (int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : defaultPath;

    if (argc <= 1) writeFile (path);

    try
    {
        MultiPartInputFile file (path);
        const Header&      hdr = file.header (0);
        const BytesAttribute& b =
            hdr.typedAttribute<BytesAttribute> ("payload");
        std::cout << "read header ok: " << b.typeHint << " " << b.size ()
                  << " bytes\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what () << "\n";
        return 2;
    }
    return 0;
}
