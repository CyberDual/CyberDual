#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <Imath/half.h>

#include <zlib.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

static uint32_t load_le32(const std::vector<uint8_t>& bytes, size_t at)
{
    return uint32_t(bytes.at(at)) | (uint32_t(bytes.at(at + 1)) << 8) |
           (uint32_t(bytes.at(at + 2)) << 16) | (uint32_t(bytes.at(at + 3)) << 24);
}

static uint64_t load_le64(const std::vector<uint8_t>& bytes, size_t at)
{
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) value = (value << 8) | bytes.at(at + i);
    return value;
}

static void store_le32(std::vector<uint8_t>& bytes, size_t at, uint32_t value)
{
    for (int i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (8 * i));
}

static void store_le64(std::vector<uint8_t>& bytes, size_t at, uint64_t value)
{
    for (int i = 0; i < 8; ++i) {
        bytes.at(at + i) = uint8_t(value);
        value >>= 8;
    }
}

static size_t after_cstring(const std::vector<uint8_t>& bytes, size_t at)
{
    while (bytes.at(at) != 0) ++at;
    return at + 1;
}

int main()
{
    const char* path = "/tmp/openexr_dwa_short_rle.exr";
    try {
        Header header(32, 32);
        header.compression() = DWAA_COMPRESSION;
        header.channels().insert("A", Channel(HALF));
        std::vector<half> pixels(32 * 32, half(0.0f));
        FrameBuffer frame;
        frame.insert("A", Slice(HALF, reinterpret_cast<char*>(pixels.data()),
                                sizeof(half), 32 * sizeof(half)));
        {
            OutputFile output(path, header);
            output.setFrameBuffer(frame);
            output.writePixels(32);
        }

        std::ifstream input(path, std::ios::binary);
        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>{input}, {});
        size_t at = 8;
        while (bytes.at(at) != 0) {
            at = after_cstring(bytes, at);
            at = after_cstring(bytes, at);
            uint32_t size = load_le32(bytes, at);
            at += 4 + size;
        }
        size_t chunk = size_t(load_le64(bytes, at + 1));
        size_t payload = chunk + 8;
        uint64_t unknown = load_le64(bytes, payload + 2 * 8);
        uint64_t ac = load_le64(bytes, payload + 3 * 8);
        uint64_t dc = load_le64(bytes, payload + 4 * 8);
        uint64_t old_rle = load_le64(bytes, payload + 5 * 8);
        if (load_le64(bytes, payload) != 2 || old_rle == 0)
            throw std::runtime_error("generated file does not contain DWA RLE data");
        size_t rule_size = size_t(bytes.at(payload + 88)) |
                           (size_t(bytes.at(payload + 89)) << 8);
        size_t rle_at = payload + 88 + rule_size + unknown + ac + dc;

        const uint8_t short_rle[] = {0, 0};
        uLongf compressed_size = compressBound(sizeof(short_rle));
        std::vector<uint8_t> compressed(compressed_size);
        if (compress2(compressed.data(), &compressed_size, short_rle,
                      sizeof(short_rle), 9) != Z_OK)
            throw std::runtime_error("zlib compression failed");
        compressed.resize(compressed_size);

        bytes.erase(bytes.begin() + rle_at, bytes.begin() + rle_at + old_rle);
        bytes.insert(bytes.begin() + rle_at, compressed.begin(), compressed.end());
        store_le64(bytes, payload + 5 * 8, compressed.size());
        store_le64(bytes, payload + 6 * 8, sizeof(short_rle));
        store_le64(bytes, payload + 7 * 8, 1);
        store_le32(bytes, chunk + 4,
                   uint32_t(load_le32(bytes, chunk + 4) - old_rle + compressed.size()));
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        output.close();

        try {
            InputFile decoded(path);
            decoded.setFrameBuffer(frame);
            decoded.readPixels(0, 31);
            std::cerr << "malformed DWA stream was accepted\n";
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "reader rejected malformed stream: " << e.what() << '\n';
            return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << "probe setup failed: " << e.what() << '\n';
        return 2;
    }
}
