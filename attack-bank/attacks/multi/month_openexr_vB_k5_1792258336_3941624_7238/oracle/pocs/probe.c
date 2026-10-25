#include "ImfIDManifest.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace
{

void
put32 (std::vector<char>& bytes, uint32_t value)
{
    bytes.push_back (char (value));
    bytes.push_back (char (value >> 8));
    bytes.push_back (char (value >> 16));
    bytes.push_back (char (value >> 24));
}

void
putVariableInteger (std::vector<char>& bytes, uint64_t value)
{
    do
    {
        unsigned char byte = (unsigned char) (value & 127);
        value >>= 7;
        if (value != 0) { byte |= 128; }
        bytes.push_back (char (byte));
    } while (value != 0);
}

void
putStringList (std::vector<char>& bytes, const char* first, const char* second)
{
    put32 (bytes, 2);
    putVariableInteger (bytes, 1);
    putVariableInteger (bytes, 1);
    bytes.push_back (*first);
    bytes.push_back (*second);
}

void
putPrefixedStringList (std::vector<char>& bytes)
{
    put32 (bytes, 2);
    putVariableInteger (bytes, 1);
    putVariableInteger (bytes, 2);
    bytes.push_back ('A');
    bytes.push_back (char (0));
    bytes.push_back ('B');
}

} // namespace

int
main (void)
{
    std::vector<char> data;

    put32 (data, 0);
    putPrefixedStringList (data);

    put32 (data, 1);
    put32 (data, 0);
    put32 (data, 0);

    put32 (data, 1);
    putStringList (data, "C", "D");
    putStringList (data, "E", "F");
    data.push_back (char (0));
    put32 (data, 0);
    put32 (data, 0);
    data.push_back (char (2));
    put32 (data, 1);
    putVariableInteger (data, 0);
    putVariableInteger (data, 1);
    putVariableInteger (data, 1);

    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (
            data.data (), data.data () + data.size ());
        return manifest.size () == 1 ? 0 : 1;
    }
    catch (...)
    {
        return 0;
    }
}
