#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BDF_H

static void
put16( unsigned char*  p,
       unsigned        v )
{
  p[0] = (unsigned char)( v >> 8 );
  p[1] = (unsigned char)v;
}

static void
put32( unsigned char*  p,
       unsigned        v )
{
  p[0] = (unsigned char)( v >> 24 );
  p[1] = (unsigned char)( v >> 16 );
  p[2] = (unsigned char)( v >> 8 );
  p[3] = (unsigned char)v;
}

/*
 * Minimal TrueType face with one embedded 'BDF ' strike.  The property
 * name is a normal NUL-terminated atom ("FOO"); its value occupies the
 * rest of the string table and is not NUL-terminated.  A size is selected
 * so the strike's ppem matches, then the property is queried.
 */
static unsigned char*
build_font( size_t*  size )
{
  static const unsigned char  head[54] = {
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x5F, 0x0F, 0x3C, 0xF5,
    0x00, 0x00,
    0x04, 0x00,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x03, 0x00,
    0x00, 0x00,
    0x00, 0x08,
    0x00, 0x02,
    0x00, 0x00,
    0x00, 0x00
  };
  static const unsigned char  hhea[36] = {
    0x00, 0x01, 0x00, 0x00,
    0x02, 0x00,
    0xFE, 0x00,
    0x00, 0x00,
    0x02, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x01
  };
  static const unsigned char  hmtx[4] = { 0x02, 0x00, 0x00, 0x00 };
  static const unsigned char  loca[4] = { 0x00, 0x00, 0x00, 0x00 };
  static const unsigned char  maxp[6] = { 0x00, 0x00, 0x50, 0x00, 0x00, 0x01 };

  const unsigned  value_len = 16;
  unsigned char   bdf[64];
  unsigned        bdf_len;
  unsigned        strings_off;
  unsigned        off, file_size;
  unsigned        off_bdf, off_head, off_hhea, off_hmtx, off_loca, off_maxp;
  unsigned char*  font;
  unsigned char*  dir;

  memset( bdf, 0, sizeof( bdf ) );
  strings_off = 22;
  put16( bdf + 0, 0x0001 );
  put16( bdf + 2, 1 );
  put32( bdf + 4, strings_off );
  put16( bdf + 8, 12 );
  put16( bdf + 10, 1 );
  put32( bdf + 12, 0 );
  put16( bdf + 16, 0x0011 );
  put32( bdf + 18, 4 );
  memcpy( bdf + strings_off, "FOO", 3 );
  bdf[strings_off + 3] = 0;
  memset( bdf + strings_off + 4, 'A', value_len );
  bdf_len = strings_off + 4 + value_len;

  off = 12 + 6 * 16;
  off_bdf  = off;                 /* filled once the data offsets are known */
  off_head = off;
  off = ( off_head + (unsigned)sizeof( head ) + 3u ) & ~3u;
  off_hhea = off;
  off = off_hhea + (unsigned)sizeof( hhea );
  off_hmtx = off;
  off = off_hmtx + (unsigned)sizeof( hmtx );
  off_loca = off;
  off = off_loca + (unsigned)sizeof( loca );
  off_maxp = off;
  off = ( off_maxp + (unsigned)sizeof( maxp ) + 3u ) & ~3u;
  off_bdf = off;
  file_size = off_bdf + bdf_len;

  font = (unsigned char*)malloc( file_size );
  if ( !font )
    return NULL;
  memset( font, 0, file_size );

  put32( font + 0, 0x00010000 );
  put16( font + 4, 6 );
  put16( font + 6, 64 );          /* searchRange */
  put16( font + 8, 2 );           /* entrySelector */
  put16( font + 10, 32 );         /* rangeShift */

  dir = font + 12;
  put32( dir +  0, 0x42444620 ); put32( dir +  4, 0 );
  put32( dir +  8, off_bdf );    put32( dir + 12, bdf_len );
  put32( dir + 16, 0x68656164 ); put32( dir + 20, 0 );
  put32( dir + 24, off_head );   put32( dir + 28, (unsigned)sizeof( head ) );
  put32( dir + 32, 0x68686561 ); put32( dir + 36, 0 );
  put32( dir + 40, off_hhea );   put32( dir + 44, (unsigned)sizeof( hhea ) );
  put32( dir + 48, 0x686D7478 ); put32( dir + 52, 0 );
  put32( dir + 56, off_hmtx );   put32( dir + 60, (unsigned)sizeof( hmtx ) );
  put32( dir + 64, 0x6C6F6361 ); put32( dir + 68, 0 );
  put32( dir + 72, off_loca );   put32( dir + 76, (unsigned)sizeof( loca ) );
  put32( dir + 80, 0x6D617870 ); put32( dir + 84, 0 );
  put32( dir + 88, off_maxp );   put32( dir + 92, (unsigned)sizeof( maxp ) );

  memcpy( font + off_head, head, sizeof( head ) );
  memcpy( font + off_hhea, hhea, sizeof( hhea ) );
  memcpy( font + off_hmtx, hmtx, sizeof( hmtx ) );
  memcpy( font + off_loca, loca, sizeof( loca ) );
  memcpy( font + off_maxp, maxp, sizeof( maxp ) );
  memcpy( font + off_bdf, bdf, bdf_len );

  *size = file_size;
  return font;
}

int
main( void )
{
  FT_Library       library;
  FT_Face          face;
  FT_Error         error;
  BDF_PropertyRec  prop;
  unsigned char*   font;
  size_t           font_size;

  font = build_font( &font_size );
  if ( !font )
  {
    fprintf( stderr, "font alloc failed\n" );
    return 1;
  }

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed: %d\n", (int)error );
    free( font );
    return 1;
  }

  error = FT_New_Memory_Face( library, font, (FT_Long)font_size, 0, &face );
  if ( error )
  {
    fprintf( stderr, "FT_New_Memory_Face failed: %d\n", (int)error );
    FT_Done_FreeType( library );
    free( font );
    return 1;
  }

  error = FT_Set_Pixel_Sizes( face, 0, 12 );
  if ( error )
  {
    fprintf( stderr, "FT_Set_Pixel_Sizes failed: %d\n", (int)error );
    FT_Done_Face( face );
    FT_Done_FreeType( library );
    free( font );
    return 1;
  }

  memset( &prop, 0, sizeof( prop ) );
  error = FT_Get_BDF_Property( face, "FOO", &prop );
  fprintf( stderr, "FT_Get_BDF_Property -> %d type %d\n",
           (int)error, (int)prop.type );

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( font );
  return 0;
}
