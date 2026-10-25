#include <ImfBytesAttribute.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfMultiPartInputFile.h>
#include <ImfOutputFile.h>

#include <cstdio>
#include <vector>

int
main ()
{
    using namespace OPENEXR_IMF_NAMESPACE;

    const char* path = "/tmp/openexr-large-bytes-attribute.exr";
    std::remove (path);

    Header header (1, 1);
    header.channels ().insert (
        "F",
        Channel (OPENEXR_IMF_INTERNAL_NAMESPACE::FLOAT, 1, 1));

    std::vector<unsigned char> bytes (256, 0x41);
    header.insert ("custom", BytesAttribute (bytes.size (), bytes.data ()));

    float pixel = 0.f;
    FrameBuffer framebuffer;
    framebuffer.insert (
        "F",
        Slice (
            OPENEXR_IMF_INTERNAL_NAMESPACE::FLOAT,
            reinterpret_cast<char*> (&pixel),
            sizeof (float),
            sizeof (float),
            1,
            1));

    {
        OutputFile output (path, header);
        output.setFrameBuffer (framebuffer);
        output.writePixels (1);
    }

    MultiPartInputFile input (path);
    return input.parts () == 1 ? 0 : 1;
}
