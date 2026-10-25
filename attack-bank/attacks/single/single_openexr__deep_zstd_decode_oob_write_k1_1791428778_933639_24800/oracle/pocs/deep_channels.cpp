#include <ImfChannelList.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfDeepScanLineInputFile.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfHeader.h>
#include <ImfPartType.h>
#include <ImathBox.h>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

int main ()
{
    using namespace OPENEXR_IMF_NAMESPACE;
    using namespace IMATH_NAMESPACE;
    const int width = 64;
    const int height = 32;
    const int channels = 33;
    const char* path = "/tmp/openexr-deep-channels.exr";
    Header header (width, height);
    header.setType (DEEPSCANLINE);
    header.compression () = ZSTD_COMPRESSION;
    for (int c = 0; c < channels; ++c)
        header.channels ().insert (std::to_string (c), Channel (FLOAT));

    std::vector<unsigned int> counts (width * height, 1);
    std::array<std::vector<float>, channels> values;
    std::array<std::vector<float*>, channels> pointers;
    DeepFrameBuffer frame;
    frame.insertSampleCountSlice (Slice (
        UINT, reinterpret_cast<char*> (counts.data ()),
        sizeof (unsigned int), width * sizeof (unsigned int)));
    for (int c = 0; c < channels; ++c)
    {
        values[c].resize (width * height, 0.0f);
        pointers[c].resize (width * height);
        for (int p = 0; p < width * height; ++p)
            pointers[c][p] = &values[c][p];
        frame.insert (std::to_string (c), DeepSlice (
            FLOAT, reinterpret_cast<char*> (pointers[c].data ()),
            sizeof (float*), width * sizeof (float*), sizeof (float)));
    }
    {
        DeepScanLineOutputFile output (path, header, 1);
        output.setFrameBuffer (frame);
        output.writePixels (height);
    }
    {
        DeepScanLineInputFile input (path, 1);
        input.setFrameBuffer (frame);
        input.readPixelSampleCounts (0, height - 1);
        input.readPixels (0, height - 1);
    }
    std::remove (path);
    return 0;
}
