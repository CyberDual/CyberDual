#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <string.h>

static void
put16( unsigned char* p, unsigned int value )
{
  p[0] = (unsigned char)value;
  p[1] = (unsigned char)( value >> 8 );
}

static void
put32( unsigned char* p, unsigned int value )
{
  put16( p, value );
  put16( p + 2, value >> 16 );
}

int
main( void )
{
  unsigned char  file[0x394];
  unsigned char* fnt = file + 0x300;
  FT_Library    library;
  FT_Face       face;
  FT_Error      error;

  memset( file, 0, sizeof ( file ) );
  put16( file, 0x5A4D );
  put32( file + 0x3C, 0x80 );

  put32( file + 0x80, 0x4550 );
  put16( file + 0x84, 0x14C );
  put16( file + 0x86, 1 );
  put16( file + 0x94, 0xE0 );
  put16( file + 0x98, 0x10B );
  put32( file + 0x108, 0x1000 );
  put32( file + 0x10C, 0x200 );

  memcpy( file + 0x178, ".rsrc", 5 );
  put32( file + 0x184, 0x1000 );
  put32( file + 0x188, 0x200 );
  put32( file + 0x18C, 0x200 );

  put16( file + 0x20E, 1 );
  put32( file + 0x210, 8 );
  put32( file + 0x214, 0x80000020U );
  put16( file + 0x22E, 1 );
  put32( file + 0x230, 1 );
  put32( file + 0x234, 0x80000040U );
  put16( file + 0x24E, 1 );
  put32( file + 0x250, 0x409 );
  put32( file + 0x254, 0x60 );
  put32( file + 0x260, 0x1100 );
  put32( file + 0x264, 148 );

  put16( fnt, 0x300 );
  put32( fnt + 2, 180 );
  put16( fnt + 68, 12 );
  put16( fnt + 74, 8 );
  put16( fnt + 88, 8 );
  put16( fnt + 91, 8 );
  put16( fnt + 93, 8 );
  put32( fnt + 105, 147 );
  fnt[147] = 'A';

  error = FT_Init_FreeType( &library );
  if ( error )
    return 2;

  error = FT_New_Memory_Face( library, file, sizeof ( file ), 0, &face );
  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  printf( "FT_New_Memory_Face: %d\n", error );
  return 0;
}
