#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H


int
main( void )
{
  static const char  base[] =
    "%!PS-AdobeFont-1.0: Test 001.001\n"
    "/FontName /Test def\n"
    "/PaintType 0 def\n"
    "/FontType 1 def\n"
    "/FontMatrix [0.001 0 0 0.001 0 0] def\n"
    "/FontBBox {-100 -100 1000 1000} def\n";

  /* One binary eexec segment.  Decrypts to whitespace. */
  static const unsigned char  cipher[15] =
  {
    0xf9, 0x83, 0xef, 0x00, 0xd3, 0xda, 0x63, 0xf3,
    0x9a, 0xf0, 0xb3, 0xee, 0x8a, 0x12, 0x46
  };

  unsigned char  pfb[512];
  unsigned char* p = pfb;
  FT_Library     library;
  FT_Face        face = NULL;
  unsigned long  blen = (unsigned long)strlen( base );


  *p++ = 0x80;
  *p++ = 0x01;
  *p++ = (unsigned char)( blen       );
  *p++ = (unsigned char)( blen >>  8 );
  *p++ = (unsigned char)( blen >> 16 );
  *p++ = (unsigned char)( blen >> 24 );
  memcpy( p, base, blen );
  p += blen;

  *p++ = 0x80;
  *p++ = 0x02;
  *p++ = (unsigned char)sizeof ( cipher );
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  memcpy( p, cipher, sizeof ( cipher ) );
  p += sizeof ( cipher );

  *p++ = 0x80;
  *p++ = 0x03;

  if ( FT_Init_FreeType( &library ) )
    return 1;

  (void)FT_New_Memory_Face( library, pfb, (FT_Long)( p - pfb ), 0, &face );

  if ( face )
    FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}
