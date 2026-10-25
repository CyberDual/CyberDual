#include <ImfChannelList.h>
#include <ImfCompression.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfPartType.h>
#include <ImathBox.h>
#include <openexr.h>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <half.h>
#include <iostream>
#include <string>
#include <vector>

int
main ()
{
    const std::string path   = "openexr-zstd-deep-residual.exr";
    const int         width  = 1;
    const int         height = 1;

    Imath::Box2i displayWindow (
        Imath::V2i (0, 0), Imath::V2i (width - 1, height - 1));
    Imath::Box2i dataWindow = displayWindow;
    Imf::Header  header (
        displayWindow,
        dataWindow,
        1,
        Imath::V2f (0, 0),
        1,
        Imf::INCREASING_Y,
        Imf::ZSTD_COMPRESSION);
    header.channels ().insert ("H", Imf::Channel (Imf::HALF));
    header.setType (Imf::DEEPSCANLINE);

    std::vector<unsigned int> sampleCounts (width * height, 64);
    std::vector<half*> halfData (width * height);
    for (int i = 0; i < width * height; ++i)
    {
        halfData[i] = new half[64];
        for (int sample = 0; sample < 64; ++sample)
            halfData[i][sample] = half (sample + 1);
    }

    {
        Imf::DeepFrameBuffer framebuffer;
        framebuffer.insertSampleCountSlice (Imf::Slice (
            Imf::UINT,
            reinterpret_cast<char*> (sampleCounts.data ()),
            sizeof (unsigned int),
            sizeof (unsigned int) * width));
        framebuffer.insert (
            "H",
            Imf::DeepSlice (
                Imf::HALF,
                reinterpret_cast<char*> (halfData.data ()),
                sizeof (half*),
                sizeof (half*) * width,
                sizeof (half)));

        Imf::DeepScanLineOutputFile output (path.c_str (), header, 1);
        output.setFrameBuffer (framebuffer);
        output.writePixels (height);
    }

    for (int i = 0; i < width * height; ++i)
        delete[] halfData[i];

    uint64_t leaderEnd = 0;
    {
        exr_context_t     context;
        exr_chunk_info_t  chunk;
        exr_start_read (&context, path.c_str (), nullptr);
        exr_read_scanline_chunk_info (context, 0, 0, &chunk);
        leaderEnd = chunk.sample_count_data_offset;
        exr_finish (&context);
    }

    std::fstream stream (path, std::ios::in | std::ios::out | std::ios::binary);
    stream.seekg (static_cast<std::streamoff> (leaderEnd - 8));
    uint64_t originalSize = 0;
    stream.read (reinterpret_cast<char*> (&originalSize), sizeof (originalSize));
    std::cout << "original unpacked size: " << originalSize << "\n";
    stream.seekp (static_cast<std::streamoff> (leaderEnd - 8));
    const uint64_t reducedSize = originalSize - 1;
    stream.write (
        reinterpret_cast<const char*> (&reducedSize), sizeof (reducedSize));
    stream.close ();

    {
        exr_context_t    context;
        exr_chunk_info_t chunk;
        exr_start_read (&context, path.c_str (), nullptr);
        exr_read_scanline_chunk_info (context, 0, 0, &chunk);
        std::cout << "mutated unpacked size: " << chunk.unpacked_size << "\n";
        exr_finish (&context);
    }

    exr_context_t     context;
    exr_chunk_info_t  chunk;
    exr_start_read (&context, path.c_str (), nullptr);
    exr_read_scanline_chunk_info (context, 0, 0, &chunk);

    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;
    exr_decoding_initialize (context, 0, &chunk, &decoder);
    exr_decoding_choose_default_routines (context, 0, &decoder);
    const exr_result_t result = exr_decoding_run (context, 0, &decoder);
    exr_decoding_destroy (context, &decoder);
    exr_finish (&context);
    std::remove (path.c_str ());

    return result == EXR_ERR_SUCCESS ? 0 : 1;
}
