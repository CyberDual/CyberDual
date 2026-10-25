  /* PoC: PFB private-dictionary segment reading.
   *
   * Build a Type 1 PFB font in memory and hand it to `FT_New_Memory_Face`:
   *
   *   [0x8001][size]  cleartext segment ("%!PS-AdobeFont-1.0: ...")
   *   [0x8002][size]   binary private-dictionary segment, complete
   *   [0x8002][size]   another binary segment whose declared size is
   *                    larger than the data actually stored in the file
   *
   * The font is a regular PFB except that the last segment header
   * announces more bytes than remain in the file -- a truncated trailing
   * segment of the kind broken font producers emit.
   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include <freetype/freetype.h>


#define BASE_LEN     96
#define PRIV1_LEN    64
#define PRIV2_STORED 64
#define PRIV2_DECL   ( PRIV2_STORED + 6 )


  static void
  put_pfb_tag( unsigned char**  p,
               unsigned short   tag,
               unsigned long    size )
  {
    *(*p)++ = (unsigned char)( tag >> 8 );
    *(*p)++ = (unsigned char)( tag & 0xFF );
    *(*p)++ = (unsigned char)( size & 0xFF );
    *(*p)++ = (unsigned char)( ( size >> 8 ) & 0xFF );
    *(*p)++ = (unsigned char)( ( size >> 16 ) & 0xFF );
    *(*p)++ = (unsigned char)( ( size >> 24 ) & 0xFF );
  }


  int
  main( void )
  {
    unsigned char*  font;
    unsigned char*  p;
    size_t          size;
    int             i;
    FT_Library      library;
    FT_Face         face = NULL;


    size = 3 * 6 + BASE_LEN + PRIV1_LEN + PRIV2_STORED;
    font = (unsigned char*)malloc( size );
    if ( !font )
      return 1;
    p = font;

    put_pfb_tag( &p, 0x8001, BASE_LEN );
    memcpy( p, "%!PS-AdobeFont-1.0: Test 001.001", 31 );
    memset( p + 31, 'x', BASE_LEN - 31 );
    p += BASE_LEN;

    put_pfb_tag( &p, 0x8002, PRIV1_LEN );
    for ( i = 0; i < PRIV1_LEN; i++ )
      *p++ = (unsigned char)( i * 7 + 3 );

    put_pfb_tag( &p, 0x8002, PRIV2_DECL );
    memset( p, 0xAA, PRIV2_STORED );
    p += PRIV2_STORED;

    if ( FT_Init_FreeType( &library ) )
    {
      free( font );
      return 2;
    }

    printf( "FT_New_Memory_Face -> %d\n",
            (int)FT_New_Memory_Face( library,
                                     font,
                                     (FT_Long)( p - font ),
                                     0,
                                     &face ) );

    if ( face )
      FT_Done_Face( face );
    FT_Done_FreeType( library );
    free( font );

    printf( "no crash\n" );
    return 0;
  }
