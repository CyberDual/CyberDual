#include "ImfIDManifest.h"

#include <cstddef>
#include <vector>

// Two-string ID manifest. The first string is longer than 255 bytes, so the
// second string's common-prefix length is the 16-bit form. The declared
// prefix is one past the previous string.
int
main ()
{
    std::vector<char> blob;
    blob.reserve (269);

    const unsigned char header[] = {
        0x00, 0x00, 0x00, 0x00, // version
        0x02, 0x00, 0x00, 0x00, // number of strings
        0x80, 0x02,             // length 256
        0x02,                   // length 2
    };
    blob.insert (blob.end (), header, header + sizeof (header));

    blob.insert (blob.end (), 256, 'A');

    blob.push_back (0x01); // common = 0x0101 = 257
    blob.push_back (0x01);

    try
    {
        Imf::IDManifest manifest (
            blob.data (), blob.data () + blob.size ());
        (void) manifest;
    }
    catch (...)
    {
        return 0;
    }
    return 0;
}
