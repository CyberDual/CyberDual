#include <ImfChannelList.h>
#include <ImfCompression.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static uint32_t read32 (const std::vector<uint8_t>& data, size_t at)
{
    if (at + 4 > data.size ()) throw std::runtime_error ("short EXR file");
    return uint32_t (data[at]) | (uint32_t (data[at + 1]) << 8) |
           (uint32_t (data[at + 2]) << 16) | (uint32_t (data[at + 3]) << 24);
}

static uint64_t read64 (const std::vector<uint8_t>& data, size_t at)
{
    return uint64_t (read32 (data, at)) | (uint64_t (read32 (data, at + 4)) << 32);
}

static void write32 (std::vector<uint8_t>& data, size_t at, int32_t value)
{
    if (at + 4 > data.size ()) throw std::runtime_error ("short EXR file");
    uint32_t bits = static_cast<uint32_t> (value);
    for (int i = 0; i < 4; ++i) data[at + i] = uint8_t (bits >> (i * 8));
}

static std::string readName (const std::vector<uint8_t>& data, size_t& at)
{
    size_t begin = at;
    while (at < data.size () && data[at] != 0) ++at;
    if (at == data.size ()) throw std::runtime_error ("short EXR header");
    std::string name (reinterpret_cast<const char*> (data.data () + begin), at - begin);
    ++at;
    return name;
}

static void moveWindow (std::vector<uint8_t>& data)
{
    size_t at = 8;
    bool found = false;
    while (true)
    {
        std::string name = readName (data, at);
        if (name.empty ()) break;
        std::string type = readName (data, at);
        uint32_t size = read32 (data, at);
        at += 4;
        if (size > data.size () - at) throw std::runtime_error ("short EXR attribute");
        if (name == "dataWindow" && type == "box2i" && size == 16)
        {
            write32 (data, at + 4, -2);
            write32 (data, at + 12, 29);
            found = true;
        }
        at += size;
    }
    if (!found) throw std::runtime_error ("missing data window");
    uint64_t chunk = read64 (data, at);
    if (chunk > data.size () - 4) throw std::runtime_error ("bad chunk offset");
    write32 (data, static_cast<size_t> (chunk), -2);
}

int main ()
{
    const int width = 128;
    const int height = 32;
    const std::string filename = "/tmp/htj2k-residual-" + std::to_string (getpid ()) + ".exr";
    try
    {
        Header header (width, height);
        header.compression () = HTJ2K32_COMPRESSION;
        header.channels ().insert ("Y", Channel (HALF, 1, 2));
        std::vector<uint16_t> pixels (width * height / 2, 0);
        FrameBuffer frame;
        frame.insert ("Y", Slice (HALF, reinterpret_cast<char*> (pixels.data ()),
                                  sizeof (uint16_t), width * sizeof (uint16_t), 1, 2));
        {
            OutputFile output (filename.c_str (), header);
            output.setFrameBuffer (frame);
            output.writePixels (height);
        }

        std::ifstream in (filename, std::ios::binary);
        std::vector<uint8_t> data ((std::istreambuf_iterator<char> (in)),
                                   std::istreambuf_iterator<char> ());
        moveWindow (data);
        {
            std::ofstream out (filename, std::ios::binary | std::ios::trunc);
            out.write (reinterpret_cast<const char*> (data.data ()), data.size ());
        }

        std::vector<uint16_t> decoded (width * height / 2, 0);
        FrameBuffer target;
        target.insert ("Y", Slice (HALF,
                                   reinterpret_cast<char*> (decoded.data () + width),
                                   sizeof (uint16_t), width * sizeof (uint16_t), 1, 2));
        InputFile input (filename.c_str ());
        input.setFrameBuffer (target);
        input.readPixels (-2, 29);
        std::remove (filename.c_str ());
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf (stderr, "%s\n", error.what ());
        std::remove (filename.c_str ());
        return 2;
    }
}
