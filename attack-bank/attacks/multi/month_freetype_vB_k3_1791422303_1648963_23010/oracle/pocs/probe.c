#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdlib.h>
#include <string.h>

int
main( void )
{
  static const unsigned char base[] =
    "%!PS-AdobeFont-1.0: Probe 1.0\n"
    "/FontName /Probe def\n"
    "/FontType 1 def\n";
  unsigned char bytes[sizeof ( base ) + 32];
  unsigned char* font;
  unsigned int pos = 0;
  FT_Library library;
  FT_Face face;
  unsigned int n = (unsigned int)strlen( (const char*)base );

  bytes[pos++] = 0x80;
  bytes[pos++] = 0x01;
  bytes[pos++] = (unsigned char)n;
  bytes[pos++] = (unsigned char)( n >> 8 );
  bytes[pos++] = (unsigned char)( n >> 16 );
  bytes[pos++] = (unsigned char)( n >> 24 );
  memcpy( bytes + pos, base, n );
  pos += n;
  bytes[pos++] = 0x80;
  bytes[pos++] = 0x02;
  bytes[pos++] = 8;
  bytes[pos++] = 0;
  bytes[pos++] = 0;
  bytes[pos++] = 0;
  memcpy( bytes + pos, "abcdefgh", 7 );
  pos += 7;

  font = (unsigned char*)malloc( pos );
  if ( !font )
    return 1;
  memcpy( font, bytes, pos );

  if ( FT_Init_FreeType( &library ) )
    return 1;
  if ( !FT_New_Memory_Face( library, font, (FT_Long)pos, 0, &face ) )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( font );
  return 0;
}
