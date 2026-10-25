// SPDX-License-Identifier: BSD-3-Clause
// Round-trip a wide lossy LJ2K scanline image through the public API.

#include <ImfChannelList.h>
#include <ImfCompression.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <half.h>

#include <cstdint>
#include <iostream>
#include <vector>

int
main ()
{
    using namespace OPENEXR_IMF_NAMESPACE;

    const int W = 300000;
    const int H = 4;

    std::vector<half> r ((size_t) W * H);
    std::vector<half> g (r.size ());
    std::vector<half> b (r.size ());
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            const size_t i = (size_t) y * W + x;
            r[i]            = half (0.25f + 0.50f * (float) (x % 1024) / 1023.f);
            g[i]            = half (0.15f + 0.10f * (float) y);
            b[i]            = half (0.40f);
        }
    }

    const char* path = "/tmp/lj2k_wide_row.exr";
    Header      hdr (W, H);
    hdr.compression () = LJ2K_COMPRESSION;
    hdr.channels ().insert ("R", Channel (HALF));
    hdr.channels ().insert ("G", Channel (HALF));
    hdr.channels ().insert ("B", Channel (HALF));

    {
        OutputFile  out (path, hdr);
        FrameBuffer fb;
        const size_t stride = sizeof (half) * (size_t) W;
        fb.insert ("R", Slice (HALF, (char*) r.data (), sizeof (half), stride));
        fb.insert ("G", Slice (HALF, (char*) g.data (), sizeof (half), stride));
        fb.insert ("B", Slice (HALF, (char*) b.data (), sizeof (half), stride));
        out.setFrameBuffer (fb);
        out.writePixels (H);
    }

    std::vector<half> rr (r.size ()), gg (g.size ()), bb (b.size ());
    {
        InputFile   in (path);
        FrameBuffer fb;
        const size_t stride = sizeof (half) * (size_t) W;
        fb.insert ("R", Slice (HALF, (char*) rr.data (), sizeof (half), stride));
        fb.insert ("G", Slice (HALF, (char*) gg.data (), sizeof (half), stride));
        fb.insert ("B", Slice (HALF, (char*) bb.data (), sizeof (half), stride));
        in.setFrameBuffer (fb);
        in.readPixels (0, H - 1);
    }

    std::cout << "round-trip " << W << "x" << H
              << " r0=" << (float) rr[0] << "\n";
    return 0;
}
