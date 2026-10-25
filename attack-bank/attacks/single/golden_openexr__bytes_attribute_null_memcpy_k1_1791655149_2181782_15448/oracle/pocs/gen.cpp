#include <ImfOutputFile.h>
#include <ImfHeader.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfBytesAttribute.h>
#include <half.h>
#include <cstdio>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

int main (int argc, char** argv)
{
    const char* out = argc > 1 ? argv[1] : "crafted.exr";
    Header header (1, 1);
    header.channels ().insert ("R", Channel (HALF));

    // zero-length bytes attribute (setData guards null, safe to write everywhere)
    BytesAttribute battr;
    battr.setData (nullptr, 0);
    header.insert ("myBytes", battr);

    OutputFile file (out, header);
    half px = 0;
    FrameBuffer fb;
    fb.insert ("R", Slice (HALF, (char*) &px, sizeof (half), 0));
    file.setFrameBuffer (fb);
    file.writePixels (1);
    printf ("wrote %s\n", out);
    return 0;
}
