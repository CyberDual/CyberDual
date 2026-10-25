#include <ImfBytesAttribute.h>
#include <ImfHeader.h>
#include <ImfMultiPartInputFile.h>
#include <ImfRgbaFile.h>

#include <vector>

using namespace Imf;

int
main ()
{
    const int         width  = 1;
    const int         height = 1;
    const std::string path   = "residual.exr";

    Header header (width, height);
    const std::vector<unsigned char> bytes = { 0 };
    header.insert (
        "data", BytesAttribute (bytes.size (), bytes.data (), "x"));

    std::vector<Rgba> pixels (static_cast<size_t> (width * height));
    {
        RgbaOutputFile output (path.c_str (), header, WRITE_RGBA);
        output.setFrameBuffer (pixels.data (), 1, 0);
        output.writePixels (height);
    }

    MultiPartInputFile input (path.c_str ());
    Header copiedHeader = input.header (0);
    return copiedHeader.hasType () ? 0 : 1;
}
