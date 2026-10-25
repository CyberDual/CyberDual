#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>

/*
 * Minimal PFB: a short Type 1 header segment followed by a private
 * segment whose recorded length is larger than the bytes that follow
 * it, while still fitting in the file.  Face construction is ordinary
 * FT_New_Memory_Face use.
 */

static void
put_u32le( unsigned char*  p,
           unsigned long   v )
{
  p[0] = (unsigned char)( v       );
  p[1] = (unsigned char)( v >>  8 );
  p[2] = (unsigned char)( v >> 16 );
  p[3] = (unsigned char)( v >> 24 );
}


int
main( void )
{
  static const char  prefix[] = "%!PS-AdobeFont-1.0: Probe\n";
  const unsigned long  ascii_len = 96;
  const unsigned long  payload   = 16;
  const unsigned long  recorded  = 64;
  unsigned char        buf[256];
  unsigned char*       p = buf;
  unsigned long        total;
  FT_Library           library;
  FT_Face              face;


  if ( ascii_len < sizeof ( prefix ) - 1 ||
       recorded <= payload               ||
       sizeof ( buf ) < 6 + ascii_len + 6 + payload )
    return 0;

  memset( buf, 0, sizeof ( buf ) );

  *p++ = 0x80;
  *p++ = 0x01;
  put_u32le( p, ascii_len );
  p += 4;
  memcpy( p, prefix, sizeof ( prefix ) - 1 );
  memset( p + sizeof ( prefix ) - 1, ' ',
          ascii_len - ( sizeof ( prefix ) - 1 ) );
  p += ascii_len;

  *p++ = 0x80;
  *p++ = 0x02;
  put_u32le( p, recorded );
  p += 4;
  memset( p, 0x5A, payload );
  p += payload;

  total = (unsigned long)( p - buf );

  if ( FT_Init_FreeType( &library ) )
    return 0;

  face = NULL;
  FT_New_Memory_Face( library, buf, total, 0, &face );

  if ( face )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
