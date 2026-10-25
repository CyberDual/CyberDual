#include <ImfIDManifest.h>

int
main (void)
{
    static const unsigned char raw[] = {
        0, 0, 0, 0, 2, 0, 0, 0, 1, 1, 120, 0, 1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 65, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};

    const char* data = reinterpret_cast<const char*> (raw);

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (
            data, data + sizeof (raw));
    }
    catch (...)
    {
        return 0;
    }

    return 0;
}
