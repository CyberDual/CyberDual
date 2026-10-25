#include "ImfIDManifest.h"

int main(void)
{
    const char data[] = {0, 0, 0, 0, 0, 0, 0, 1};
    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest(data, data + sizeof(data));
    }
    catch (...)
    {
    }
    return 0;
}
