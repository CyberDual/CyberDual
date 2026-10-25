#include "ImfChannelList.h"
#include "ImfDeepFrameBuffer.h"
#include "ImfDeepScanLineInputFile.h"
#include "ImfDeepScanLineOutputFile.h"
#include "ImfPartType.h"
#include "openexr.h"

#include <Imath/half.h>
#include <zstd.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

int main (int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "/tmp/openexr-zstd-deep-probe.exr";
    constexpr int width = 64;
    std::array<unsigned int, width> counts;
    std::array<half, width> values;
    std::array<half*, width> pointers;
    for (int i = 0; i < width; ++i)
    {
        counts[i] = 1;
        values[i] = half (0.5f);
        pointers[i] = &values[i];
    }

    Header header (width, 1);
    header.setType (DEEPSCANLINE);
    header.compression () = ZSTD_COMPRESSION;
    header.channels ().insert ("A", Channel (HALF));
    header.channels ().insert ("B", Channel (HALF));
    header.channels ().insert ("C", Channel (HALF));
    DeepFrameBuffer fb;
    fb.insertSampleCountSlice (Slice (UINT, reinterpret_cast<char*> (counts.data ()), sizeof (unsigned int), width * sizeof (unsigned int)));
    for (const char* name : {"A", "B", "C"})
        fb.insert (name, DeepSlice (HALF, reinterpret_cast<char*> (pointers.data ()), sizeof (half*), width * sizeof (half*), sizeof (half)));

    {
        DeepScanLineOutputFile out (path, header, 1);
        out.setFrameBuffer (fb);
        out.writePixels (1);
    }

    exr_context_t ctxt = nullptr;
    if (exr_start_read (&ctxt, path, nullptr) != EXR_ERR_SUCCESS) return 2;
    exr_chunk_info_t chunk;
    if (exr_read_scanline_chunk_info (ctxt, 0, 0, &chunk) != EXR_ERR_SUCCESS) return 3;
    exr_finish (&ctxt);
    std::cerr << "packed=" << chunk.packed_size << " unpacked=" << chunk.unpacked_size << " table=" << chunk.sample_count_table_size << "\n";
    std::ifstream input (path, std::ios::binary);
    std::vector<unsigned char> bytes ((std::istreambuf_iterator<char> (input)), std::istreambuf_iterator<char> ());
    input.close ();
    const size_t offset = static_cast<size_t> (chunk.data_offset);
    const size_t frame_size = static_cast<size_t> (chunk.packed_size) - 24;
    const auto inner_size = ZSTD_getFrameContentSize (bytes.data () + offset + 24, frame_size);
    if (inner_size == ZSTD_CONTENTSIZE_ERROR || inner_size == ZSTD_CONTENTSIZE_UNKNOWN) return 4;
    std::vector<unsigned char> inner (static_cast<size_t> (inner_size) + 2);
    if (ZSTD_isError (ZSTD_decompress (inner.data (), inner_size, bytes.data () + offset + 24, frame_size))) return 5;
    const uint64_t longer = chunk.unpacked_size + 2;
    for (int i = 0; i < 8; ++i) inner[i] = static_cast<unsigned char> (longer >> (i * 8));
    inner[inner_size] = 0;
    inner[inner_size + 1] = 0;
    std::vector<unsigned char> compressed (ZSTD_compressBound (inner.size ()));
    const size_t compressed_size = ZSTD_compress (compressed.data (), compressed.size (), inner.data (), inner.size (), 3);
    if (ZSTD_isError (compressed_size)) return 6;
    compressed.resize (compressed_size);
    const uint64_t packed_size = compressed_size + 24;
    for (int i = 0; i < 8; ++i)
    {
        bytes[static_cast<size_t> (chunk.sample_count_data_offset - 16 + i)] = static_cast<unsigned char> (packed_size >> (i * 8));
        bytes[offset + 16 + i] = static_cast<unsigned char> (compressed_size >> (i * 8));
    }
    bytes.resize (offset + 24);
    bytes.insert (bytes.end (), compressed.begin (), compressed.end ());
    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    output.write (reinterpret_cast<const char*> (bytes.data ()), bytes.size ());
    output.close ();

    try
    {
        DeepScanLineInputFile in (path, 1);
        in.setFrameBuffer (fb);
        in.readPixels (0);
        std::cerr << "decoded\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "rejected: " << e.what () << "\n";
    }
    return 0;
}
