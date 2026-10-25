#include <ft2build.h>
#include FT_FREETYPE_H

static const unsigned char font[75] = {
  128, 1, 44, 0, 0, 0, 37, 33, 80, 83, 45, 65, 100, 111, 98, 101, 70, 111, 110, 116, 45, 49, 46, 48, 58, 32, 80, 114, 111, 98, 101, 10, 99, 117, 114, 114, 101, 110, 116, 102, 105, 108, 101, 32, 101, 101, 120, 101, 99, 10, 128, 2, 0, 0, 0, 0, 128, 2, 4, 0, 0, 0, 65, 65, 65, 65, 128, 1, 1, 0, 0, 0, 10, 128, 3
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
