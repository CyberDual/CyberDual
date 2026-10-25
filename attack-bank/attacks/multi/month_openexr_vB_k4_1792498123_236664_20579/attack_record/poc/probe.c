/*
 * Probe: feed a crafted, serialized IDManifest blob to the public
 * OpenEXR parser constructor Imf::IDManifest(const char*, const char*).
 *
 * The blob below is a syntactically well-formed manifest:
 *   - version 0
 *   - a string table of two strings ("a" and, after prefix decoding, "x")
 *   - an RLE mapping table that covers only string-table slot 0, leaving
 *     slot 1 unmapped (the format does not require full coverage)
 *   - zero channel-group manifests
 *
 * Usage: probe [file]   (with no argument, uses the embedded blob)
 *
 * Exits 0 when the manifest is parsed or cleanly rejected; aborts under
 * AddressSanitizer if the library reads out of bounds while parsing.
 */

#include <ImfIDManifest.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

using OPENEXR_IMF_NAMESPACE::IDManifest;

static const unsigned char embedded_blob[] = {
    0x00, 0x00, 0x00, 0x00, /* version = 0                          */
    0x02, 0x00, 0x00, 0x00, /* string count = 2                     */
    0x01, 0x02,             /* string lengths: 1, 2                 */
    'a',  0x00, 'x',        /* string data                          */
    0x01, 0x00, 0x00, 0x00, /* RLE mapping table entries = 1        */
    0x00, 0x00, 0x00, 0x00, /*   first = 0                          */
    0x00, 0x00, 0x00, 0x00, /*   last = 0  (only slot 0 is mapped)  */
    0x00, 0x00, 0x00, 0x00  /* channel-group manifest count = 0     */
};

int
main (int argc, char** argv)
{
    std::vector<char> buf;

    const char* data = (const char*) embedded_blob;
    size_t      size = sizeof (embedded_blob);

    if (argc > 1)
    {
        FILE* f = fopen (argv[1], "rb");
        if (!f)
        {
            fprintf (stderr, "cannot open %s\n", argv[1]);
            return 2;
        }
        fseek (f, 0, SEEK_END);
        long n = ftell (f);
        fseek (f, 0, SEEK_SET);
        if (n <= 0)
        {
            fclose (f);
            return 2;
        }
        buf.resize ((size_t) n);
        if (fread (buf.data (), 1, (size_t) n, f) != (size_t) n)
        {
            fclose (f);
            return 2;
        }
        fclose (f);
        data = buf.data ();
        size = buf.size ();
    }

    try
    {
        IDManifest manifest (data, data + size);
    }
    catch (...)
    {
        /* a clean parse error is a fine outcome */
        return 0;
    }

    return 0;
}
