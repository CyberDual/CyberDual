#include <OpenEXR/ImfIDManifest.h>

int main(void)
{
    const char data[] = {
        0, 0, 0, 0,    // manifest version
        1, 0, 0, 0,    // one string
        16,            // string length
        'x'            // truncated string data
    };

    try
    {
        Imf::IDManifest manifest(data, data + sizeof(data));
    }
    catch (...)
    {
    }
    return 0;
}
