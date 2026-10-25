#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>

/* Uncompressed WOFF whose synthesized SFNT is just over 64MB.
 * Structurally consistent (directory, lengths, and padding all agree);
 * only the expanded size exceeds the loader's ceiling.
 */

static void
put_u16( unsigned char*  p,
         unsigned int    v )
{
  p[0] = (unsigned char)( v >> 8 );
  p[1] = (unsigned char)( v      );
}

static void
put_u32( unsigned char*  p,
         unsigned int    v )
{
  p[0] = (unsigned char)( v >> 24 );
  p[1] = (unsigned char)( v >> 16 );
  p[2] = (unsigned char)( v >>  8 );
  p[3] = (unsigned char)( v       );
}

int
main( void )
{
  /* 12 + 1*16 = 28 byte SFNT header, then the table.  Total must be
   * strictly larger than 64MB and a multiple of 4. */
  const unsigned int  orig_len  = 0x3FFFFE8u;          /* 67108840 */
  const unsigned int  file_len  = 64u + orig_len;      /* header + table */
  const unsigned int  sfnt_size = 28u + orig_len;      /* 0x4000004     */
  unsigned char*      buf;
  FILE*               fp;
  FT_Library          library;
  FT_Face             face;
  FT_Error            error;
  const char*         path = "/tmp/residual.woff";


  buf = (unsigned char*)calloc( 1, file_len );
  if ( !buf )
    return 1;

  put_u32( buf +  0, 0x774F4646u );  /* 'wOFF'            */
  put_u32( buf +  4, 0x00010000u );  /* sfnt version      */
  put_u32( buf +  8, file_len );
  put_u16( buf + 12, 1 );            /* numTables         */
  put_u16( buf + 14, 0 );            /* reserved          */
  put_u32( buf + 16, sfnt_size );
  put_u16( buf + 20, 1 );            /* majorVersion      */
  put_u16( buf + 22, 0 );            /* minorVersion      */
  /* metaOffset/metaLength/metaOrigLength/priv* stay zero */

  put_u32( buf + 44, 0x68656164u );  /* tag 'head'        */
  put_u32( buf + 48, 64 );           /* offset            */
  put_u32( buf + 52, orig_len );     /* compLength        */
  put_u32( buf + 56, orig_len );     /* origLength        */
  put_u32( buf + 60, 0 );            /* checksum          */

  fp = fopen( path, "wb" );
  if ( !fp )
  {
    free( buf );
    return 1;
  }
  if ( fwrite( buf, 1, file_len, fp ) != file_len )
  {
    fclose( fp );
    free( buf );
    return 1;
  }
  fclose( fp );
  free( buf );

  error = FT_Init_FreeType( &library );
  if ( error )
    return 1;

  error = FT_New_Face( library, path, 0, &face );
  fprintf( stderr, "FT_New_Face -> 0x%02x\n", (unsigned)error );

  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
