/* newbug.c
 *
 * Builds a minimal TrueType font carrying a 'BDF ' table in memory,
 * then queries a BDF property through the public FreeType API:
 *
 *   FT_New_Memory_Face + FT_Set_Pixel_Sizes + FT_Get_BDF_Property
 *
 * The 'BDF ' table's string table holds the property name as its last
 * bytes without a terminating NUL, and the table ends exactly at the
 * end of the font image.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BDF_H

static void
put16( unsigned char*  b,
       size_t          o,
       unsigned int    v )
{
  b[o]     = (unsigned char)( v >> 8 );
  b[o + 1] = (unsigned char)( v & 0xFF );
}

static void
put32( unsigned char*  b,
       size_t          o,
       unsigned long   v )
{
  b[o]     = (unsigned char)( v >> 24 );
  b[o + 1] = (unsigned char)( ( v >> 16 ) & 0xFF );
  b[o + 2] = (unsigned char)( ( v >> 8 ) & 0xFF );
  b[o + 3] = (unsigned char)( v & 0xFF );
}

static size_t
align4( size_t  v )
{
  return ( v + 3 ) & ~(size_t)3;
}

/* Builds the font; returns a malloc'd buffer, and sets *font_len and
 * *ppem_off (file offset of the strike ppem field in the BDF table). */
static unsigned char*
build_font( size_t*  font_len,
            size_t*  ppem_off )
{
  /* property name, unterminated, occupying the whole string table */
  const char*  prop_string     = "ABCDEF";
  size_t       prop_string_len = 6;

  /* BDF table layout: header(8) + strikes(4) + records(10) + strings */
  size_t  bdf_strings_off = 8 + 4 + 10;
  size_t  bdf_len         = bdf_strings_off + prop_string_len;

  /* physical table order: head hhea maxp hmtx cmap loca glyf BDF */
  size_t  off         = align4( 12 + 8 * 16 );
  size_t  head_off    = off;  off = align4( off + 54 );
  size_t  hhea_off    = off;  off = align4( off + 36 );
  size_t  maxp_off    = off;  off = align4( off + 32 );
  size_t  hmtx_off    = off;  off = align4( off + 4 );
  size_t  cmap_off    = off;  off = align4( off + 274 );
  size_t  loca_off    = off;  off = align4( off + 4 );
  size_t  glyf_off    = off;  /* length 0 */
  size_t  bdf_off     = off;  off = off + bdf_len;   /* BDF ends at EOF */

  size_t          total = off;
  unsigned char*  f     = calloc( 1, total );

  if ( !f )
    exit( 1 );

  /* sfnt offset table */
  put32( f, 0, 0x00010000UL );
  put16( f, 4, 8 );      /* numTables */
  put16( f, 6, 128 );    /* searchRange */
  put16( f, 8, 3 );      /* entrySelector */
  put16( f, 10, 0 );     /* rangeShift */

  /* directory entries sorted by tag */
  {
    size_t  d = 12;

#define DIRENTRY( tag, toff, tlen )                     \
    put32( f, d, tag ); put32( f, d + 4, 0 );           \
    put32( f, d + 8, (unsigned long)( toff ) );         \
    put32( f, d + 12, (unsigned long)( tlen ) );        \
    d += 16;

    DIRENTRY( 0x42444620UL, bdf_off,  bdf_len )  /* 'BDF ' */
    DIRENTRY( 0x636D6170UL, cmap_off, 274 )      /* 'cmap' */
    DIRENTRY( 0x676C7966UL, glyf_off, 0 )        /* 'glyf' */
    DIRENTRY( 0x68656164UL, head_off, 54 )       /* 'head' */
    DIRENTRY( 0x68686561UL, hhea_off, 36 )       /* 'hhea' */
    DIRENTRY( 0x686D7478UL, hmtx_off, 4 )        /* 'hmtx' */
    DIRENTRY( 0x6C6F6361UL, loca_off, 4 )        /* 'loca' */
    DIRENTRY( 0x6D617870UL, maxp_off, 32 )       /* 'maxp' */
#undef DIRENTRY
  }

  /* head */
  put32( f, head_off + 0,  0x00010000UL );  /* version */
  put32( f, head_off + 4,  0x00010000UL );  /* fontRevision */
  put32( f, head_off + 8,  0 );             /* checkSumAdjustment */
  put32( f, head_off + 12, 0x5F0F3CF5UL );  /* magicNumber */
  put16( f, head_off + 16, 0 );             /* flags */
  put16( f, head_off + 18, 1000 );          /* unitsPerEm */
  put16( f, head_off + 44, 8 );             /* lowestRecPPEM */
  put16( f, head_off + 46, 2 );             /* fontDirectionHint */
  put16( f, head_off + 48, 0 );             /* indexToLocFormat: short */
  put16( f, head_off + 50, 0 );             /* glyphDataFormat */

  /* hhea */
  put32( f, hhea_off + 0,  0x00010000UL );
  put16( f, hhea_off + 4,  800 );   /* ascender */
  put16( f, hhea_off + 6,  (unsigned int)(unsigned short)( -200 ) );
  put16( f, hhea_off + 10, 500 );   /* advanceWidthMax */
  put16( f, hhea_off + 24, 1 );     /* caretSlopeRise */
  put16( f, hhea_off + 34, 1 );     /* numberOfHMetrics */

  /* maxp */
  put32( f, maxp_off + 0, 0x00010000UL );
  put16( f, maxp_off + 4, 1 );      /* numGlyphs */

  /* hmtx: advanceWidth 500, lsb 0 */
  put16( f, hmtx_off + 0, 500 );

  /* cmap: one format-0 subtable (platform 3, encoding 1) */
  put16( f, cmap_off + 0, 0 );      /* version */
  put16( f, cmap_off + 2, 1 );      /* numTables */
  put16( f, cmap_off + 4, 3 );
  put16( f, cmap_off + 6, 1 );
  put32( f, cmap_off + 8, 12 );
  put16( f, cmap_off + 12, 0 );     /* format 0 */
  put16( f, cmap_off + 14, 262 );   /* length */
  put16( f, cmap_off + 16, 0 );     /* language */

  /* loca: [0, 0] -- single empty glyph */
  /* glyf: empty */

  /* BDF  */
  put16( f, bdf_off + 0, 1 );                            /* version */
  put16( f, bdf_off + 2, 1 );                            /* num_strikes */
  put32( f, bdf_off + 4, (unsigned long)bdf_strings_off );
  put16( f, bdf_off + 8,  0xFFFF );        /* ppem (patched below) */
  put16( f, bdf_off + 10, 1 );                           /* count */
  put32( f, bdf_off + 12, 0 );                           /* name_offset */
  put16( f, bdf_off + 16, 0x11 );                        /* type: atom */
  put32( f, bdf_off + 18, 0 );                           /* value */
  memcpy( f + bdf_off + bdf_strings_off, prop_string, prop_string_len );

  *font_len = total;
  *ppem_off = bdf_off + 8;
  return f;
}

int
main( void )
{
  const char*  prop_name = "ABCDEF";

  size_t          font_len, ppem_off;
  unsigned char*  font = build_font( &font_len, &ppem_off );

  FT_Library  library;
  FT_Face     face;
  FT_Error    error;

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType failed: %d\n", error );
    return 1;
  }

  /* pass 1: learn the realized y_ppem for a 16px strike */
  error = FT_New_Memory_Face( library, font, (FT_Long)font_len, 0, &face );
  if ( error )
  {
    fprintf( stderr, "FT_New_Memory_Face failed: 0x%x\n", error );
    return 1;
  }
  FT_Set_Pixel_Sizes( face, 0, 16 );
  {
    unsigned int  y_ppem = (unsigned int)face->size->metrics.y_ppem;
    FT_Done_Face( face );
    put16( font, ppem_off, y_ppem );
  }

  /* pass 2: query the property */
  error = FT_New_Memory_Face( library, font, (FT_Long)font_len, 0, &face );
  if ( error )
  {
    fprintf( stderr, "FT_New_Memory_Face(2) failed: 0x%x\n", error );
    return 1;
  }
  FT_Set_Pixel_Sizes( face, 0, 16 );

  {
    BDF_PropertyRec  prop;

    error = FT_Get_BDF_Property( face, prop_name, &prop );
    printf( "FT_Get_BDF_Property(\"%s\") -> error 0x%x, type %d\n",
            prop_name, error, (int)prop.type );
  }

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  free( font );
  return 0;
}
