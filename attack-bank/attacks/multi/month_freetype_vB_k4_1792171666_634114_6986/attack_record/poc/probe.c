#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftimage.h>
#include <freetype/ftoutln.h>


int
main( void )
{
  FT_Library        library;
  FT_Outline        outline;
  FT_Bitmap         bitmap;
  FT_Raster_Params  params;
  unsigned char*    buffer;
  FT_UInt           n = 13000;
  FT_UInt           i;


  if ( FT_Init_FreeType( &library ) )
    return 1;

  if ( FT_Outline_New( library, n, (FT_Int)n, &outline ) )
  {
    FT_Done_FreeType( library );
    return 1;
  }

  for ( i = 0; i < n; i++ )
  {
    outline.points[i].x = 32 * 64;
    outline.points[i].y = 32 * 64;
    outline.tags[i]     = FT_CURVE_TAG_ON;
    outline.contours[i] = (unsigned short)i;
  }

  buffer = (unsigned char*)calloc( (size_t)400 * 400, 1 );
  if ( !buffer )
  {
    FT_Outline_Done( library, &outline );
    FT_Done_FreeType( library );
    return 1;
  }

  FT_Bitmap_Init( &bitmap );
  bitmap.buffer     = buffer;
  bitmap.width      = 400;
  bitmap.rows       = 400;
  bitmap.pitch      = 400;
  bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
  bitmap.num_grays  = 256;

  memset( &params, 0, sizeof ( params ) );
  params.target = &bitmap;
  params.source = &outline;
  params.flags  = FT_RASTER_FLAG_AA;

  (void)FT_Outline_Render( library, &outline, &params );

  free( buffer );
  FT_Outline_Done( library, &outline );
  FT_Done_FreeType( library );

  return 0;
}
