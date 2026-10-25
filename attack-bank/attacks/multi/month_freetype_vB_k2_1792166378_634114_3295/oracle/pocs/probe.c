#include <ft2build.h>
#include FT_FREETYPE_H

static const unsigned char  font[] =
{
  128, 1, 26, 0, 0, 0, 37, 33, 80, 83, 45, 65,
  100, 111, 98, 101, 70, 111, 110, 116, 45, 49, 46, 48,
  58, 32, 80, 114, 111, 98, 101, 10, 128, 2, 41, 0,
  0, 0, 129, 184, 114, 128, 229, 175, 183, 156, 72, 183,
  234, 158, 99, 200, 79, 140, 151, 17, 80, 173, 144, 4,
  158, 154, 207, 247, 178, 150, 102, 249, 114, 139, 83, 37,
  204, 108, 229, 51, 141, 58, 27, 128, 3
};

int
main( void )
{
  FT_Library  library;
  FT_Face     face;


  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( !FT_New_Memory_Face( library, font,
                            (FT_Long)sizeof ( font ), 0, &face ) )
    FT_Done_Face( face );

  FT_Done_FreeType( library );
  return 0;
}
