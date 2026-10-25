#include "ImfIDManifest.h"

#include <cstddef>
#include <exception>

int
main (void)
{
    static const unsigned char input[] = {
        0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x01,
        0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00
    };

    const char* begin = reinterpret_cast<const char*> (input);
    const char* end   = begin + sizeof (input);

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (begin, end);
        (void) manifest;
    }
    catch (const std::exception&)
    {
        return 0;
    }

    return 0;
}
