/*
 * newbug.c
 *
 * Load a small hand-built WOFF font through the public FreeType API to
 * exercise the WOFF table unpacking code path.  The font is first opened
 * from a file with FT_New_Face; if that does not yield a face, it is
 * opened again through a user-supplied read stream with FT_Open_Face,
 * which is the documented way of feeding FreeType from custom I/O.
 *
 * Build (against an ASan-instrumented FreeType):
 *
 *   cc -fsanitize=address -g -O1 -I<ft2>/include -o newbug newbug.c \
 *      <ft2>/build/libfreetype.a -lz -lm
 *
 * Usage: newbug [font-file]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freetype/freetype.h>
#include <freetype/ftsystem.h>


static void
put_u32( unsigned char*  p,
         unsigned long   v )
{
  p[0] = (unsigned char)( v >> 24 );
  p[1] = (unsigned char)( v >> 16 );
  p[2] = (unsigned char)( v >>  8 );
  p[3] = (unsigned char)( v       );
}


static void
put_u16( unsigned char*  p,
         unsigned long   v )
{
  p[0] = (unsigned char)( v >> 8 );
  p[1] = (unsigned char)( v      );
}


/*
 * A WOFF wrapper around a TrueType-flavoured sfnt that consists of a
 * single `cmap' table.  The table is flagged as compressed: its stored
 * (compressed) length is one byte only, while the header declares an
 * eight-byte original length.
 */

#define NB_NUM_TABLES    1
#define NB_ORIG_LENGTH   8
#define NB_COMP_LENGTH   1
#define NB_TABLE_OFFSET  ( 44 + 20 * NB_NUM_TABLES )          /* 64 */
#define NB_WOFF_LENGTH   ( NB_TABLE_OFFSET + 4 )              /* 68 */
#define NB_TOTAL_SFNT    ( 12 + 16 * NB_NUM_TABLES + \
                         ( ( NB_ORIG_LENGTH + 3 ) & ~3UL ) )  /* 36 */


static unsigned char*
build_woff( void )
{
  unsigned char*  woff = calloc( NB_WOFF_LENGTH, 1 );
  unsigned char*  directory;


  if ( !woff )
    return NULL;

  /* WOFF header. */
  put_u32( woff +  0, 0x774F4646UL );            /* 'wOFF'            */
  put_u32( woff +  4, 0x00010000UL );            /* flavor            */
  put_u32( woff +  8, NB_WOFF_LENGTH );          /* length            */
  put_u16( woff + 12, NB_NUM_TABLES );           /* numTables         */
  put_u16( woff + 14, 0 );                       /* reserved          */
  put_u32( woff + 16, NB_TOTAL_SFNT );           /* totalSfntSize     */
  put_u16( woff + 20, 1 );                       /* majorVersion      */
  put_u16( woff + 22, 0 );                       /* minorVersion      */
  put_u32( woff + 24, 0 );                       /* metaOffset        */
  put_u32( woff + 28, 0 );                       /* metaLength        */
  put_u32( woff + 32, 0 );                       /* metaOrigLength    */
  put_u32( woff + 36, 0 );                       /* privOffset        */
  put_u32( woff + 40, 0 );                       /* privLength        */

  /* Table directory entry. */
  directory = woff + 44;
  put_u32( directory      , 0x636D6170UL );      /* 'cmap'            */
  put_u32( directory +  4, NB_TABLE_OFFSET );    /* offset            */
  put_u32( directory +  8, NB_COMP_LENGTH );     /* compLength        */
  put_u32( directory + 12, NB_ORIG_LENGTH );     /* origLength        */
  put_u32( directory + 16, 0 );                  /* origChecksum      */

  /* Table data (plus 4-byte alignment padding). */
  woff[NB_TABLE_OFFSET] = 0x78;

  return woff;
}


static unsigned long
poc_stream_io( FT_Stream       stream,
               unsigned long   offset,
               unsigned char*  buffer,
               unsigned long   count )
{
  const unsigned char*  data = (const unsigned char*)stream->descriptor.pointer;

  if ( offset > NB_WOFF_LENGTH ||
       count > NB_WOFF_LENGTH - offset )
    return 0;

  memcpy( buffer, data + offset, count );

  return count;
}


int
main( int    argc,
      char** argv )
{
  const char*     path = argc > 1 ? argv[1] : "/tmp/newbug.woff";
  unsigned char*  woff;
  FILE*           fp;
  FT_Library      library;
  FT_Face         face;
  FT_Error        error;


  woff = build_woff();
  if ( !woff )
  {
    fprintf( stderr, "out of memory\n" );
    return 2;
  }

  fp = fopen( path, "wb" );
  if ( !fp )
  {
    fprintf( stderr, "cannot open %s\n", path );
    free( woff );
    return 2;
  }
  fwrite( woff, 1, NB_WOFF_LENGTH, fp );
  fclose( fp );

  if ( FT_Init_FreeType( &library ) )
  {
    fprintf( stderr, "cannot initialize FreeType\n" );
    free( woff );
    return 2;
  }

  /* First, open the font from a file like any client would. */
  face = NULL;
  error = FT_New_Face( library, path, 0, &face );
  printf( "FT_New_Face: %d\n", error );

  if ( !error )
  {
    printf( "family: %s\n", face->family_name ? face->family_name
                                              : "(none)" );
    FT_Done_Face( face );
    FT_Done_FreeType( library );
    free( woff );

    return 0;
  }

  /* Otherwise, open the same font through a user-provided stream. */
  {
    FT_StreamRec  stream_rec;
    FT_Open_Args  args;


    memset( &stream_rec, 0, sizeof ( stream_rec ) );
    stream_rec.size = NB_WOFF_LENGTH;
    stream_rec.descriptor.pointer = woff;
    stream_rec.read = poc_stream_io;
    stream_rec.close = NULL;

    memset( &args, 0, sizeof ( args ) );
    args.flags = FT_OPEN_STREAM;
    args.stream = &stream_rec;

    face = NULL;
    error = FT_Open_Face( library, &args, 0, &face );
    printf( "FT_Open_Face: %d\n", error );

    if ( !error )
      FT_Done_Face( face );
  }

  FT_Done_FreeType( library );
  free( woff );

  return error ? 1 : 0;
}


/* END */
