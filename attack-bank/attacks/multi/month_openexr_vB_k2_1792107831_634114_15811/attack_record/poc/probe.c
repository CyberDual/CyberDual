#include "ImfIDManifest.h"

#include <exception>

// Empty ID manifest: version 0, two strings, no mapping runs, no channel
// groups, and one trailing scheme byte. Parsed through the public constructor.
int
main (void)
{
    const char blob[] = {
        0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (
            blob, blob + sizeof (blob));
        (void) manifest;
    }
    catch (const std::exception&)
    {
        return 0;
    }
    return 0;
}
