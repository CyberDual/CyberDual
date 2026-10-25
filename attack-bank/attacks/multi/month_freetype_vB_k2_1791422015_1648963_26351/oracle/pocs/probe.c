#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>

static void
segment( unsigned char* out, size_t* used, unsigned char tag,
         const unsigned char* data, size_t length )
{
  size_t i;

  out[(*used)++] = 0x80;
  out[(*used)++] = tag;
  for ( i = 0; i < 4; i++ )
    out[(*used)++] = (unsigned char)( length >> ( 8 * i ) );
  memcpy( out + *used, data, length );
  *used += length;
}

int
main( void )
{
  static const unsigned char base[] =
    "%!PS-AdobeFont-1.0: Probe 1.0\n"
    "/FontName /Probe def\n"
    "/FontType 1 def\n"
    "/FontMatrix [0.001 0 0 0.001 0 0] def\n"
    "/FontBBox [0 0 1000 1000] def\n"
    "currentfile eexec\n";
  static const unsigned char part1[] = { 0x8a, 0x12, 0xc3, 0x4f, 0x55 };
  static const unsigned char part2[] = { 0x21, 0x8d, 0x0e, 0x6b, 0x71 };
  unsigned char pfb[512];
  size_t used = 0;
  FT_Library library;
  FT_Face face;
  FT_Error error;

  segment( pfb, &used, 1, base, sizeof ( base ) - 1 );
  segment( pfb, &used, 2, part1, sizeof ( part1 ) );
  segment( pfb, &used, 2, part2, sizeof ( part2 ) );
  pfb[used++] = 0x80;
  pfb[used++] = 0x03;

  error = FT_Init_FreeType( &library );
  if ( error )
    return 1;
  error = FT_New_Memory_Face( library, pfb, (FT_Long)used, 0, &face );
  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
