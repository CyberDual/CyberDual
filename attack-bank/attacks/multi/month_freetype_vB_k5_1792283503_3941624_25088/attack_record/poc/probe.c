#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  const size_t base_length = 14;
  const size_t declared_private_length = 16;
  const size_t available_private_length = 15;
  const size_t font_length = 6 + base_length + 6 +
                             available_private_length;
  unsigned char *font;
  size_t offset = 0;
  FT_Library library;
  FT_Face face;
  FT_Error error;


  font = (unsigned char *)malloc(font_length);
  if (!font)
    return 1;

  font[offset++] = 0x80;
  font[offset++] = 0x01;
  font[offset++] = (unsigned char)base_length;
  font[offset++] = 0;
  font[offset++] = 0;
  font[offset++] = 0;

  font[offset++] = '%';
  font[offset++] = '!';
  font[offset++] = 'P';
  font[offset++] = 'S';
  font[offset++] = '-';
  font[offset++] = 'A';
  font[offset++] = 'd';
  font[offset++] = 'o';
  font[offset++] = 'b';
  font[offset++] = 'e';
  font[offset++] = 'F';
  font[offset++] = 'o';
  font[offset++] = 'n';
  font[offset++] = 't';

  font[offset++] = 0x80;
  font[offset++] = 0x02;
  font[offset++] = (unsigned char)declared_private_length;
  font[offset++] = 0;
  font[offset++] = 0;
  font[offset++] = 0;

  for (; offset < font_length; offset++)
    font[offset] = 0;

  error = FT_Init_FreeType(&library);
  if (error)
  {
    free(font);
    return 1;
  }

  error = FT_New_Memory_Face(library, font, (FT_Long)font_length,
                             0, &face);
  printf("FT_New_Memory_Face: %d\n", (int)error);

  if (!error)
    FT_Done_Face(face);
  FT_Done_FreeType(library);
  free(font);
  return 0;
}
