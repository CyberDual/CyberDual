/*
 * Proof-of-concept input for an authorized, sandboxed code-review
 * evaluation harness (benchmark fixture; never shipped or deployed).
 *
 * It drives FreeType's public API, FT_Init_FreeType() / FT_New_Memory_Face(),
 * with in-contract input: a small, well-formed Type 1 "PFB" font.  The PFB
 * container format (Adobe TN5040) holds a sequence of tagged segments; this
 * test font has an ASCII "clear text" segment (tag 0x8001) followed by one
 * binary "private dictionary" segment (tag 0x8002).  Opening such a font
 * through the public API is ordinary, in-contract library use.
 */

#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/* clang-format off */
static const unsigned char test_font[] = {
  /* segment 1: tag 0x8001 (ASCII), length 17, little-endian */
  0x80, 0x01, 0x11, 0x00, 0x00, 0x00,
  '%', '!', 'F', 'o', 'n', 't', 'T', 'y', 'p', 'e',
  '1', ' ', 'f', 'o', 'n', 't', '\n',

  /* segment 2: tag 0x8002 (binary / private dict), length 16 */
  0x80, 0x02, 0x10, 0x00, 0x00, 0x00,
  0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
  0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};
/* clang-format on */

int
main( void )
{
  FT_Library library;
  FT_Face    face;
  FT_Error   error;

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed: %d\n", error );
    return 1;
  }

  /* In-contract call: parse a font entirely from a caller-owned buffer. */
  error = FT_New_Memory_Face( library, test_font, sizeof( test_font ),
                              0, &face );

  if ( !error )
    FT_Done_Face( face );

  FT_Done_FreeType( library );

  /*
   * A non-sanitizer build either rejects this font cleanly (error != 0)
   * or loads it; either outcome is fine here.  The property under test is
   * that parsing never corrupts memory, which AddressSanitizer verifies
   * independently of this return value.
   */
  return 0;
}
