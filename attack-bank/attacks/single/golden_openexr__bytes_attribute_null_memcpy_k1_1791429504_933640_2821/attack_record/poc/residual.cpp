#include <ImfMultiPartInputFile.h>
#include <ImfStdIO.h>

#include <cstdint>
#include <string>

static void u32(std::string& s, uint32_t v)
{
    for (int i = 0; i < 4; ++i) s += char(v >> (i * 8));
}

static void u64(std::string& s, uint64_t v)
{
    for (int i = 0; i < 8; ++i) s += char(v >> (i * 8));
}

static void name(std::string& s, const char* n)
{
    s.append(n);
    s += '\0';
}

static void attribute(std::string& s, const char* n, const char* type, const std::string& value)
{
    name(s, n);
    name(s, type);
    u32(s, static_cast<uint32_t>(value.size()));
    s += value;
}

int main()
{
    std::string exr;
    u32(exr, 20000630);
    u32(exr, 2);

    std::string channels;
    name(channels, "Y");
    u32(channels, 1);
    u32(channels, 0);
    u32(channels, 1);
    u32(channels, 1);
    channels += '\0';
    attribute(exr, "channels", "chlist", channels);
    attribute(exr, "compression", "compression", std::string(1, '\0'));

    std::string window;
    for (int i = 0; i < 4; ++i) u32(window, 0);
    attribute(exr, "dataWindow", "box2i", window);
    attribute(exr, "displayWindow", "box2i", window);
    attribute(exr, "lineOrder", "lineOrder", std::string(1, '\0'));

    std::string one;
    u32(one, 0x3f800000);
    attribute(exr, "pixelAspectRatio", "float", one);

    std::string center;
    u32(center, 0);
    u32(center, 0);
    attribute(exr, "screenWindowCenter", "v2f", center);
    attribute(exr, "screenWindowWidth", "float", one);

    std::string bytes;
    u32(bytes, 1);
    bytes += 'x';
    bytes += 'z';
    attribute(exr, "payload", "bytes", bytes);

    exr += '\0';
    u64(exr, exr.size() + 8);
    u32(exr, 0);
    u32(exr, 2);
    exr += '\0';
    exr += '\0';

    OPENEXR_IMF_NAMESPACE::StdISStream input;
    input.str(exr);
    OPENEXR_IMF_NAMESPACE::MultiPartInputFile file(input);
    return file.parts() == 1 ? 0 : 1;
}
