#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

#include <cmath>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;

int main ()
{
    const int width = 128;
    const int height = 256;
    const char* path = "/tmp/openexr_lj2k_residual.exr";
    const char* names[] = {"R", "G", "B", "Z"};
    std::vector<float> pixels[4];

    for (int c = 0; c < 4; ++c)
        pixels[c].resize (width * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            float value = 0.25f + 0.001f * y +
                          0.02f * std::sin (float (x) / 20.0f);
            pixels[0][y * width + x] = value;
            pixels[1][y * width + x] = value;
            pixels[2][y * width + x] = value;
            pixels[3][y * width + x] = 0.5f;
        }

    Header header (width, height);
    header.compression () = LJ2K_COMPRESSION;
    header.lossyHTJ2KQuality () = 60.f;
    FrameBuffer output;
    for (int c = 0; c < 4; ++c)
    {
        header.channels ().insert (names[c], Channel (FLOAT));
        output.insert (names[c], Slice (FLOAT,
            reinterpret_cast<char*> (pixels[c].data ()),
            sizeof (float), width * sizeof (float)));
    }
    {
        OutputFile file (path, header);
        file.setFrameBuffer (output);
        file.writePixels (height);
    }

    std::vector<float> decoded[4];
    FrameBuffer input;
    for (int c = 0; c < 4; ++c)
    {
        decoded[c].resize (width * height);
        input.insert (names[c], Slice (FLOAT,
            reinterpret_cast<char*> (decoded[c].data ()),
            sizeof (float), width * sizeof (float)));
    }
    InputFile file (path);
    file.setFrameBuffer (input);
    file.readPixels (0, height - 1);
    return 0;
}
