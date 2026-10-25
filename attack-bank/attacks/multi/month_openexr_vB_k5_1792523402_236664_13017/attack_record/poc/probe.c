// Probe: parse a crafted serialized IDManifest through the public
// OpenEXR API. The blob below is a version-0 serialized manifest whose
// string table declares two strings of 1 and 48 bytes, but the buffer
// ends right after the first string. A correct parser must reject this
// with a clean exception; a parser that trusts the declared lengths
// reads past the end of the heap buffer.
//
// Build (C++): g++ -x c++ -fsanitize=address probe.c -lOpenEXR -lIex

#include <ImfIDManifest.h>

#include <cstring>
#include <vector>

extern "C" const char*
__asan_default_options ()
{
    return "abort_on_error=1";
}

int
main ()
{
    static const unsigned char blob[] = {
        0x00, 0x00, 0x00, 0x00, // version = 0
        0x02, 0x00, 0x00, 0x00, // two strings in the table
        0x01, 0x30, // variable-length sizes: 1 and 48
        'A', 'B', 'C' // first string ("A"), then only 2 of 48 bytes
    };

    // parse from a heap buffer so out-of-bounds reads are caught
    std::vector<char> data (sizeof (blob));
    memcpy (data.data (), blob, sizeof (blob));

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (
            data.data (), data.data () + data.size ());
        (void) manifest;
    }
    catch (...)
    {
        // clean rejection of the malformed manifest
        return 0;
    }
    return 0;
}
