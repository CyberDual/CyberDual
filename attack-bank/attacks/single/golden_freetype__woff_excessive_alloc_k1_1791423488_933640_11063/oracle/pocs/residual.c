#define _POSIX_C_SOURCE 200809L

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static void
put16( unsigned char*  p,
       uint16_t        value )
{
  p[0] = (unsigned char)( value >> 8 );
  p[1] = (unsigned char)value;
}


static void
put32( unsigned char*  p,
       uint32_t        value )
{
  p[0] = (unsigned char)( value >> 24 );
  p[1] = (unsigned char)( value >> 16 );
  p[2] = (unsigned char)( value >> 8 );
  p[3] = (unsigned char)value;
}


int
main( void )
{
  static const uint32_t  sfnt_size  = 0x4000000U;
  static const uint32_t  table_size = 0x4000000U - 28;
  unsigned char          header[64] = { 0 };
  char                   path[] = "/tmp/woff-residual-XXXXXX";
  FT_Library             library;
  FT_Face                face;
  FT_Error               error;
  int                    fd;


  put32( header +  0, 0x774F4646U );
  put32( header +  4, 0x00010000U );
  put32( header +  8, 64 + table_size );
  put16( header + 12, 1 );
  put32( header + 16, sfnt_size );
  put16( header + 20, 1 );

  put32( header + 44, 0x6E616D65U );
  put32( header + 48, 64 );
  put32( header + 52, table_size );
  put32( header + 56, table_size );

  fd = mkstemp( path );
  if ( fd < 0 ||
       write( fd, header, sizeof ( header ) ) != sizeof ( header ) ||
       ftruncate( fd, 64 + table_size ) != 0 )
  {
    perror( "creating WOFF" );
    return 1;
  }
  close( fd );

  error = FT_Init_FreeType( &library );
  if ( error )
    return 2;

  error = FT_New_Face( library, path, 0, &face );
  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  unlink( path );

  printf( "FT_New_Face returned %d\n", error );
  return 0;
}
