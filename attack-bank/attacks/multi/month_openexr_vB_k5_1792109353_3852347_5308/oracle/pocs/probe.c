// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.
//
// PoC: build an IDManifest (public Imf API) with more than 64 channel
// group manifests, serialize it, then re-parse the serialized bytes
// through the public raw-bytes constructor. This mirrors how an
// application decodes the contents of an EXR file's "idManifest"
// attribute, which stores exactly this serialized byte blob.

#include <ImfIDManifest.h>

#include <vector>
#include <cstdio>
#include <cstring>

int
main (void)
{
    using namespace OPENEXR_IMF_NAMESPACE;

    IDManifest mfst;

    // One channel-group manifest per "AOV"; use more than the typical
    // handful (65) so the declared entry count is in-contract but
    // unusually large.
    for (int i = 0; i < 65; ++i)
    {
        char name[32];
        std::snprintf (name, sizeof (name), "chan%d", i);

        IDManifest::ChannelGroupManifest& g = mfst.add (name);
        g.setComponent ("value");
        g.setHashScheme (IDManifest::NOTHASHED);
        g.setEncodingScheme (IDManifest::ID_SCHEME);
        g.setLifetime (IDManifest::LIFETIME_STABLE);
    }

    std::vector<char> data;
    mfst.serialize (data);

    // feed the serialized bytes back through the public raw-bytes
    // constructor, exactly as the library does when reading an
    // idManifest attribute out of an EXR file.
    IDManifest decoded (data.data (), data.data () + data.size ());

    std::printf ("decoded %zu channel group manifests\n", decoded.size ());
    return 0;
}
