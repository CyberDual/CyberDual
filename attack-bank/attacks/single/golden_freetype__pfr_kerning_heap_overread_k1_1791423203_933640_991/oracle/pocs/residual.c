#include <ft2build.h>
#include FT_FREETYPE_H
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put16(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)(v >> 8);
  p[1] = (unsigned char)v;
}

static void put24(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)(v >> 16);
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)v;
}

static unsigned long read_font(FT_Stream stream, unsigned long offset,
                               unsigned char *buffer, unsigned long count) {
  const unsigned char *font = stream->descriptor.pointer;
  if (offset > stream->size) return count ? 0 : 1;
  if (!count) return 0;
  if (count > stream->size - offset) count = stream->size - offset;
  memcpy(buffer, font + offset, count);
  return count;
}

int main(void) {
  unsigned char font[160] = {0};
  size_t pos = 65;
  size_t phys_start, phys_size, gps_start;
  FT_StreamRec stream = {0};
  FT_Open_Args args = {0};
  FT_Library library;
  FT_Face face;
  FT_Vector kerning;

  memcpy(font, "PFR0", 4);
  put16(font + 4, 1);
  put16(font + 6, 0x0d0a);
  put16(font + 8, 58);
  put16(font + 10, 7);
  put16(font + 12, 58);
  put16(font + 14, 18);
  put24(font + 16, 18);
  put24(font + 19, 65);
  put16(font + 52, 1);
  put16(font + 56, 3);

  put16(font + 58, 1);
  put16(font + 60, 18);
  put24(font + 62, 65);

  put24(font + pos, 0x010000); pos += 3;
  pos += 6;
  put24(font + pos, 0x010000); pos += 3;
  pos++;
  assert(pos == 78);
  pos += 5;
  phys_start = pos;

  put16(font + pos, 1); pos += 2;
  put16(font + pos, 1000); pos += 2;
  put16(font + pos, 1000); pos += 2;
  pos += 8;
  font[pos++] = 0x80;
  put16(font + pos, 500); pos += 2;
  font[pos++] = 1;
  font[pos++] = 13;
  font[pos++] = 4;
  font[pos++] = 3;
  put16(font + pos, 0); pos += 2;
  font[pos++] = 0;
  font[pos++] = 10; font[pos++] = 10; font[pos++] = 1;
  font[pos++] = 20; font[pos++] = 20; font[pos++] = 2;
  font[pos++] = 30; font[pos++] = 30; font[pos++] = 3;
  pos += 3;
  font[pos++] = 0;
  pos += 6;
  put16(font + pos, 3); pos += 2;
  for (unsigned int code = 10; code <= 30; code += 10) {
    font[pos++] = (unsigned char)code;
    font[pos++] = 1;
    put16(font + pos, 1); pos += 2;
  }
  phys_size = pos - phys_start;
  gps_start = pos;
  font[pos++] = 0;
  pos += 8;
  assert(pos <= sizeof(font));

  put16(font + 22, (unsigned int)phys_size);
  put24(font + 24, (unsigned int)phys_size);
  put24(font + 27, (unsigned int)phys_start);
  put16(font + 30, 1);
  put24(font + 32, 1);
  put24(font + 35, (unsigned int)gps_start);
  put16(font + 78, (unsigned int)phys_size);
  put24(font + 80, (unsigned int)phys_start);

  stream.size = pos;
  stream.descriptor.pointer = font;
  stream.read = read_font;
  args.flags = FT_OPEN_STREAM;
  args.stream = &stream;

  if (FT_Init_FreeType(&library)) return 2;
  if (FT_Open_Face(library, &args, 0, &face)) return 3;
  if (FT_Get_Kerning(face, 3, 3, FT_KERNING_UNSCALED, &kerning)) return 4;
  if (kerning.x != 3) return 5;
  printf("kerning: %ld\n", kerning.x);
  FT_Done_Face(face);
  FT_Done_FreeType(library);
  return 0;
}
