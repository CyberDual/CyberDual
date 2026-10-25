#include <ImfBytesAttribute.h>
#include <ImfHeader.h>
#include <ImfStdIO.h>

int main ()
{
    using namespace OPENEXR_IMF_NAMESPACE;

    const unsigned char data[] = {0x42};
    Header source (1, 1);
    source.insert ("sample", BytesAttribute (sizeof (data), data, "x"));

    StdOSStream output;
    source.writeTo (output);

    StdISStream input;
    input.str (output.str ());
    Header result (1, 1);
    int version = 2;
    result.readFrom (input, version);
    return 0;
}
