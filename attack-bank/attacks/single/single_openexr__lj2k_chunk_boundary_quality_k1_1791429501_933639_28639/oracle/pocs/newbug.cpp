#include <ImfRgbaFile.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main ()
{
    using namespace OPENEXR_IMF_NAMESPACE;
    const char* path = "/tmp/lj2k-boundary.exr";
    const int width = 256;
    const int height = 128;

    std::vector<Rgba> pixels (width * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            Rgba& pixel = pixels[y * width + x];
            pixel.r = half (0.25f + 0.5f * x / width);
            pixel.g = half (0.25f + 0.5f * y / height);
            pixel.b = half (0.5f);
            pixel.a = half (1.0f);
        }

    Header header (width, height);
    header.compression () = LJ2K_COMPRESSION;
    {
        RgbaOutputFile out (path, header, WRITE_RGBA);
        out.setFrameBuffer (pixels.data (), 1, width);
        out.writePixels (height);
    }

    std::ifstream input (path, std::ios::binary);
    std::vector<char> bytes ((std::istreambuf_iterator<char> (input)),
                             std::istreambuf_iterator<char> ());
    const std::string key ("dataWindow\0box2i\0", 17);
    auto at = std::search (bytes.begin (), bytes.end (), key.begin (), key.end ());
    if (at == bytes.end ()) return 2;
    size_t max_x_offset = static_cast<size_t> (at - bytes.begin ()) + key.size () + 12;
    if (max_x_offset + 4 > bytes.size ()) return 3;
    uint32_t max_x = 382;
    for (int i = 0; i < 4; ++i)
        bytes[max_x_offset + i] = static_cast<char> (max_x >> (8 * i));
    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    output.write (bytes.data (), bytes.size ());
    output.close ();

    try
    {
        RgbaInputFile in (path);
        const auto& dw = in.dataWindow ();
        int read_width = dw.max.x - dw.min.x + 1;
        std::vector<Rgba> decoded (read_width * (dw.max.y - dw.min.y + 1));
        in.setFrameBuffer (decoded.data (), 1, read_width);
        in.readPixels (dw.min.y, dw.max.y);
    }
    catch (const std::exception&)
    {
        return 0;
    }
    return 0;
}
