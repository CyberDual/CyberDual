#include <ImfIDManifest.h>

int main(void)
{
    const unsigned char bytes[] = {
        0, 0, 0, 0,      // manifest version
        2, 0, 0, 0,      // two strings
        0x80, 0x80, 0x02,
        0x80, 0x80, 0x02
    };
    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest(
            reinterpret_cast<const char*>(bytes),
            reinterpret_cast<const char*>(bytes + sizeof(bytes)));
    }
    catch (...) {}
    return 0;
}
