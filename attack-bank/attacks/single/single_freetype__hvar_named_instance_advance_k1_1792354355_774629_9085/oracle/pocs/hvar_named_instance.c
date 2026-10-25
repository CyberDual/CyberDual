/*
 * PoC: query HVAR-adjusted advances for a named instance of a variable
 * font.  Loads an embedded minimal variable font (one wght axis, two
 * named instances, HVAR with a partial advance-width mapping), selects
 * the "Black" named instance, then queries advances for all glyphs via
 * FT_Get_Advance / FT_Get_Advances.
 */
#include <stdio.h>
#include <stdlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_ADVANCES_H

#include "font_array.h"

#define NUM_GLYPHS 8

int
main( void )
{
  FT_Library  library;
  FT_Face     face;
  FT_Error    error;
  FT_UInt     g;
  FT_Fixed    advances[NUM_GLYPHS];


  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed (%d)\n", error );
    return 1;
  }

  error = FT_New_Memory_Face( library,
                              font_data,
                              (FT_Long)sizeof ( font_data ),
                              0,
                              &face );
  if ( error )
  {
    fprintf( stderr, "FT_New_Memory_Face failed (%d)\n", error );
    return 1;
  }

  /* select the second named instance ("Black", wght=900) */
  error = FT_Set_Named_Instance( face, 2 );
  if ( error )
  {
    fprintf( stderr, "FT_Set_Named_Instance failed (%d)\n", error );
    return 1;
  }

  /* query the advance of each glyph individually */
  for ( g = 0; g < NUM_GLYPHS; g++ )
  {
    FT_Fixed  advance = 0;


    error = FT_Get_Advance( face, g, FT_LOAD_NO_SCALE, &advance );
    if ( error )
    {
      fprintf( stderr, "FT_Get_Advance(%u) failed (%d)\n", g, error );
      return 1;
    }

    printf( "glyph %u: advance %ld\n",
            (unsigned long)g,
            (long)advance );
  }

  /* query all advances at once */
  error = FT_Get_Advances( face, 0, NUM_GLYPHS, FT_LOAD_NO_SCALE, advances );
  if ( error )
  {
    fprintf( stderr, "FT_Get_Advances failed (%d)\n", error );
    return 1;
  }

  for ( g = 0; g < NUM_GLYPHS; g++ )
    printf( "advances[%u] = %ld\n",
            (unsigned long)g,
            (long)advances[g] );

  FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}
