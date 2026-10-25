#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BDF_H

#include "newbug_font.h"

int
main( void )
{
  FT_Library  library;
  FT_Face     face;
  FT_Error    error;
  BDF_PropertyRec  prop;


  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed: %d\n", error );
    return 1;
  }

  error = FT_New_Memory_Face( library, poc_font, (FT_Long)poc_font_len,
                               0, &face );
  if ( error )
  {
    fprintf( stderr, "FT_New_Memory_Face failed: %d\n", error );
    return 1;
  }

  error = FT_Set_Pixel_Sizes( face, 10, 10 );
  if ( error )
  {
    fprintf( stderr, "FT_Set_Pixel_Sizes failed: %d\n", error );
    return 1;
  }

  memset( &prop, 0, sizeof ( prop ) );
  error = FT_Get_BDF_Property( face, "a", &prop );

  printf( "FT_Get_BDF_Property returned %d, type %d\n", error, prop.type );

  FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}