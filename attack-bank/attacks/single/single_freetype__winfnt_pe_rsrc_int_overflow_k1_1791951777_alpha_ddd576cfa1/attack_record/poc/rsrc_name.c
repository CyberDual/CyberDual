#include <ft2build.h>
#include <freetype/freetype.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void
put_u16( unsigned char* p,
         unsigned int   v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
}


static void
put_u32( unsigned char* p,
         unsigned int   v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}


/*
 * PE/COFF .FON whose resource root has a single named entry.  The entry's
 * name is a length-prefixed UTF-16 string.  The resource section's raw size
 * covers the length prefix and only part of the character data; the file
 * itself continues and holds the whole string.
 */
static unsigned char*
build_font( size_t* size )
{
  const unsigned int  lfanew      = 64;
  const unsigned int  section_off = 352; /* lfanew + 4 + 20 + 0xE0 + 40 */
  const unsigned int  raw_size    = 28;
  const unsigned int  name_off    = 24;
  const unsigned int  nchars      = 4;
  const char*         payload     = "ABCDEFGH"; /* nchars UTF-16 code units */
  unsigned char*      buf;
  unsigned int        pe, opt, sec, rs;
  size_t              total;


  total = section_off + name_off + 2 + 8;
  buf   = (unsigned char*)calloc( 1, total );
  if ( !buf )
    return NULL;

  buf[0] = 'M';
  buf[1] = 'Z';
  put_u32( buf + 0x3C, lfanew );

  pe = lfanew;
  buf[pe + 0] = 'P';
  buf[pe + 1] = 'E';
  put_u16( buf + pe + 4, 0x014C ); /* machine */
  put_u16( buf + pe + 6, 1 );      /* number of sections */
  put_u16( buf + pe + 20, 0xE0 );  /* size of optional header */

  opt = pe + 24;
  put_u16( buf + opt, 0x10B );     /* PE32 */
  put_u32( buf + opt + 112, 0 );   /* resource VA */
  put_u32( buf + opt + 116, raw_size );

  sec = opt + 0xE0;
  memcpy( buf + sec, ".rsrc", 5 );
  put_u32( buf + sec + 12, 0 );          /* virtual address */
  put_u32( buf + sec + 16, raw_size );   /* size of raw data */
  put_u32( buf + sec + 20, section_off ); /* pointer to raw data */

  rs = section_off;
  put_u16( buf + rs + 12, 1 ); /* number of named entries */
  put_u16( buf + rs + 14, 0 ); /* number of id entries */
  put_u32( buf + rs + 16, 0x80000000u | name_off );
  put_u32( buf + rs + 20, 0 ); /* not a subdirectory */
  put_u16( buf + rs + name_off, nchars );
  memcpy( buf + rs + name_off + 2, payload, 8 );

  *size = total;
  return buf;
}


int
main( void )
{
  FT_Library      library;
  FT_Face         face;
  unsigned char*  font;
  size_t          font_size;
  FT_Error        error;


  font = build_font( &font_size );
  if ( !font )
    return 1;

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    free( font );
    return 1;
  }

  error = FT_New_Memory_Face( library, font, (FT_Long)font_size, 0, &face );
  if ( !error )
    FT_Done_Face( face );

  FT_Done_FreeType( library );
  free( font );

  return error ? 0 : 0;
}
