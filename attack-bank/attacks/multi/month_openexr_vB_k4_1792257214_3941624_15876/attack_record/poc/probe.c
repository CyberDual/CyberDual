#include <ImfIDManifest.h>

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>
#include <zlib.h>

int main(void)
{
    z_stream stream = {};
    if (deflateInit(&stream, 1) != Z_OK)
        return 1;

    std::vector<unsigned char> compressed;
    compressed.resize(5 * 1024 * 1024);
    stream.next_out = compressed.data();
    stream.avail_out = static_cast<unsigned int>(compressed.size());

    unsigned char input[64 * 1024] = {};
    do
    {
        stream.next_in = input;
        stream.avail_in = sizeof(input);
        if (deflate(&stream, Z_NO_FLUSH) != Z_OK)
            return 1;
    } while (stream.avail_in == 0);

    do
    {
        if (stream.avail_out == 0)
        {
            compressed.resize(compressed.size() + 1024 * 1024);
            stream.next_out = compressed.data() + stream.total_out;
            stream.avail_out = static_cast<unsigned int>(
                compressed.size() - stream.total_out);
        }
    } while (deflate(&stream, Z_FINISH) == Z_OK);

    deflateEnd(&stream);
    compressed.resize(stream.total_out);

    unsigned char* compressedData =
        static_cast<unsigned char*> (malloc (compressed.size ()));
    if (compressedData == nullptr) return 1;
    memcpy (compressedData, compressed.data (), compressed.size ());

    OPENEXR_IMF_NAMESPACE::CompressedIDManifest manifest;
    manifest._data = compressedData;
    manifest._compressedDataSize = static_cast<int>(compressed.size());
    manifest._uncompressedDataSize =
        static_cast<size_t>(UINT_MAX) + 1024 * 1024;

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest decoded(manifest);
        (void) decoded;
    }
    catch (const std::exception&)
    {
        return 0;
    }
    return 0;
}
