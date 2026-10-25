#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <unistd.h>

int main()
{
    using namespace OPENEXR_IMF_NAMESPACE;

    char path[] = "/tmp/openexr-ht-header-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 2;
    close(fd);

    try
    {
        Header header(16, 16);
        header.compression() = HTJ2K256_COMPRESSION;
        header.channels().insert("Y", Channel(FLOAT));

        float pixels[16 * 16] = {};
        FrameBuffer fb;
        fb.insert("Y", Slice(FLOAT, reinterpret_cast<char*>(pixels),
                             sizeof(float), 16 * sizeof(float)));
        {
            OutputFile out(path, header);
            out.setFrameBuffer(fb);
            out.writePixels(16);
        }

        std::ifstream in(path, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
        const unsigned char chunk_header[] = {
            'H', 'T', 0, 0, 0, 4, 0, 1, 0, 0
        };
        size_t pos = 0;
        for (; pos + sizeof(chunk_header) <= bytes.size(); ++pos)
        {
            bool match = true;
            for (size_t i = 0; i < sizeof(chunk_header); ++i)
                if (bytes[pos + i] != chunk_header[i]) match = false;
            if (match) break;
        }
        if (pos + sizeof(chunk_header) > bytes.size())
            throw std::runtime_error("HTJ2K chunk header not found");
        bytes[pos + sizeof(chunk_header) - 1] = 1;

        std::ofstream damaged(path, std::ios::binary | std::ios::trunc);
        damaged.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        damaged.close();

        InputFile input(path);
        input.setFrameBuffer(fb);
        input.readPixels(0, 15);
    }
    catch (const std::exception&)
    {
    }

    std::remove(path);
    return 0;
}
