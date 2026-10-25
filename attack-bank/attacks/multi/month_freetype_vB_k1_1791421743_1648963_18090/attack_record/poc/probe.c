#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <string.h>

int main(void) {
  static const unsigned char base[] = "%!PS-AdobeFont-1.0: Fixture 1.0\n/FontName /Fixture def\neexec\n";
  unsigned char data[sizeof(base) + 32];
  FT_Library lib;
  FT_Face face = NULL;
  size_t n = 0;
  size_t len = sizeof(base) - 1;
  FT_Error err;
  data[n++] = 0x80; data[n++] = 0x01;
  data[n++] = len & 255; data[n++] = (len >> 8) & 255;
  data[n++] = 0; data[n++] = 0;
  memcpy(data+n, base, len); n += len;
  data[n++] = 0x80; data[n++] = 0x02;
  data[n++] = 4; data[n++] = 0; data[n++] = 0; data[n++] = 0;
  data[n++] = 1; data[n++] = 2; data[n++] = 3; data[n++] = 4;
  data[n++] = 0x80;
  err = FT_Init_FreeType(&lib);
  if (err) return 2;
  err = FT_New_Memory_Face(lib, data, (FT_Long)n, 0, &face);
  printf("error=%d face=%p\n", err, (void*)face);
  if (!err) FT_Done_Face(face);
  FT_Done_FreeType(lib);
  return 0;
}
