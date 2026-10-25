#include <ft2build.h>
#include FT_FREETYPE_H

#include <stddef.h>
#include <string.h>

static void
put_segment( unsigned char* out, size_t* length,
             unsigned char kind, const unsigned char* data, size_t count )
{
  size_t i = *length;

  out[i++] = 0x80;
  out[i++] = kind;
  out[i++] = (unsigned char)count;
  out[i++] = (unsigned char)( count >> 8 );
  out[i++] = (unsigned char)( count >> 16 );
  out[i++] = (unsigned char)( count >> 24 );
  memcpy( out + i, data, count );
  *length = i + count;
}

int
main( void )
{
  static const unsigned char base[] =
    "%!PS-AdobeFont-1.0: Probe 1.0\n"
    "/FontName /Probe def\n"
    "/FontType 1 def\n"
    "/Encoding StandardEncoding def\n"
    "currentfile eexec\n";
  static const unsigned char plain[] =
    "abcd /Private 1 dict dup begin /lenIV 4 def end";
  unsigned char file[512];
  unsigned char cipher[sizeof plain - 1];
  unsigned short seed = 55665;
  size_t length = 0;
  size_t i;
  FT_Library library;
  FT_Face face;

  for ( i = 0; i < sizeof cipher; i++ )
  {
    unsigned char c = (unsigned char)( plain[i] ^ ( seed >> 8 ) );
    cipher[i] = c;
    seed = (unsigned short)( ( c + seed ) * 52845U + 22719U );
  }

  put_segment( file, &length, 1, base, sizeof base - 1 );
  put_segment( file, &length, 2, cipher, 17 );
  put_segment( file, &length, 2, cipher + 17, sizeof cipher - 17 );
  file[length++] = 0x80;
  file[length++] = 3;

  if ( FT_Init_FreeType( &library ) )
    return 1;
  if ( !FT_New_Memory_Face( library, file, (FT_Long)length, 0, &face ) )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
