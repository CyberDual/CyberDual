#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_STROKER_H


  int
  main( void )
  {
    FT_Library  library;
    FT_Stroker  stroker = NULL;
    FT_Outline  outline;
    FT_UInt     num_points   = 0;
    FT_UInt     num_contours = 0;


    if ( FT_Init_FreeType( &library ) )
      return 1;

    if ( FT_Outline_New( library, 1, 1, &outline ) )
    {
      FT_Done_FreeType( library );
      return 1;
    }

    outline.points[0].x   = 64;
    outline.points[0].y   = 64;
    outline.tags[0]       = FT_CURVE_TAG_ON;
    outline.contours[0]   = 0;

    if ( FT_Stroker_New( library, &stroker ) )
    {
      FT_Outline_Done( library, &outline );
      FT_Done_FreeType( library );
      return 1;
    }

    FT_Stroker_Set( stroker, 32 * 64,
                    FT_STROKER_LINECAP_BUTT,
                    FT_STROKER_LINEJOIN_ROUND,
                    16 * 64 );

    if ( FT_Stroker_ParseOutline( stroker, &outline, 0 ) )
    {
      FT_Stroker_Done( stroker );
      FT_Outline_Done( library, &outline );
      FT_Done_FreeType( library );
      return 1;
    }

    if ( FT_Stroker_GetCounts( stroker, &num_points, &num_contours ) )
    {
      FT_Stroker_Done( stroker );
      FT_Outline_Done( library, &outline );
      FT_Done_FreeType( library );
      return 1;
    }

    FT_Stroker_Done( stroker );
    FT_Outline_Done( library, &outline );
    FT_Done_FreeType( library );

    return 0;
  }
