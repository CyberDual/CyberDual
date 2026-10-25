#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdlib.h>
#include <string.h>

static void segment(unsigned char **p, unsigned char tag,
                    const unsigned char *data, unsigned long size)
{
  unsigned char *q = *p;
  *q++ = 0x80;
  *q++ = tag;
  *q++ = (unsigned char)size;
  *q++ = (unsigned char)(size >> 8);
  *q++ = (unsigned char)(size >> 16);
  *q++ = (unsigned char)(size >> 24);
  if (data && size) {
    memcpy(q, data, size);
    q += size;
  }
  *p = q;
}

int main(void)
{
  static const unsigned char base[] =
    "%!PS-AdobeFont-1.0: Test 1.0\n"
    "/FontName /Test def\n"
    "/FontType 1 def\n"
    "/FontMatrix [0.001 0 0 0.001 0 0] def\n"
    "currentfile eexec\n";
  static const unsigned char priv[] = { 0x8b, 0x7c, 0x12, 0xe3 };
  size_t len = 6 + sizeof(base) - 1 + 6 + sizeof(priv) + 6;
  unsigned char *buf = malloc(len), *p = buf;
  FT_Library lib;
  FT_Face face;
  int err;
  if (!buf) return 2;
  segment(&p, 1, base, sizeof(base) - 1);
  segment(&p, 2, priv, sizeof(priv));
  segment(&p, 2, NULL, 1);
  if ((size_t)(p - buf) != len) return 3;
  if (FT_Init_FreeType(&lib)) return 4;
  err = FT_New_Memory_Face(lib, buf, (FT_Long)len, 0, &face);
  if (!err) FT_Done_Face(face);
  FT_Done_FreeType(lib);
  free(buf);
  return 0;
}
