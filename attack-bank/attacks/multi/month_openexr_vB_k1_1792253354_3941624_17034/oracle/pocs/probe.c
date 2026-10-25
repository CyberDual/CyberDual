#include "ImfIDManifest.h"

#include <cstddef>

int
main (void)
{
    static const unsigned char data[] = {
        0x00, 0x00, 0x00, 0x00, // manifest version
        0x02, 0x00, 0x00, 0x00, // string list size
        0x01,                   // encoded length of "a"
        0x02,                   // encoded length of "\0b"
        'a', 0x00, 'b',         // string table
        0x01, 0x00, 0x00, 0x00, // mapping RLE entry count
        0x00, 0x00, 0x00, 0x00, // first mapped string index
        0x00, 0x00, 0x00, 0x00  // last mapped string index
    };

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (
            reinterpret_cast<const char*> (data),
            reinterpret_cast<const char*> (data) + sizeof (data));
    }
    catch (...)
    {
    }

    return 0;
}
