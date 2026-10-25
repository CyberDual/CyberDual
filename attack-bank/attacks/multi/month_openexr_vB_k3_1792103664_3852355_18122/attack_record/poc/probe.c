#include <OpenEXR/ImfIDManifest.h>

#include <exception>
#include <vector>

int main(void)
{
    // Version 0, one string, a declared string length of 32, and one byte
    // of string data.
    const std::vector<char> blob = {0, 0, 0, 0,
                                    1, 0, 0, 0,
                                    32, 'a'};
    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest(blob.data(),
                                                   blob.data() + blob.size());
    }
    catch (const std::exception&)
    {
    }
    return 0;
}
