#include "ImfArray.h"
#include "ImfBytesAttribute.h"
#include "ImfChannelList.h"
#include "ImfFrameBuffer.h"
#include "ImfHeader.h"
#include "ImfMultiPartInputFile.h"
#include "ImfOutputFile.h"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

int
main ()
{
    const char* path = "/tmp/bytes_hint_and_data.exr";
    const int   w = 1, h = 1;
    Array2D<float> px (h, w);
    px[0][0] = 0.5f;

    std::vector<unsigned char> payload = {
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

    Header hdr (w, h);
    hdr.channels ().insert ("R", Channel (FLOAT));
    hdr.insert (
        "userData",
        BytesAttribute (payload.size (), payload.data (), "application/octet-stream"));

    {
        FrameBuffer fb;
        fb.insert (
            "R",
            Slice (FLOAT, (char*) &px[0][0], sizeof (float), sizeof (float)));
        OutputFile out (path, hdr);
        out.setFrameBuffer (fb);
        out.writePixels (h);
    }

    MultiPartInputFile in (path);
    const BytesAttribute& got =
        in.header (0).typedAttribute<BytesAttribute> ("userData");
    std::cout << "size=" << got.size () << " hint=" << got.typeHint << std::endl;
    if (got.size () != payload.size () || got.typeHint != "application/octet-stream")
        return 1;
    return 0;
}
