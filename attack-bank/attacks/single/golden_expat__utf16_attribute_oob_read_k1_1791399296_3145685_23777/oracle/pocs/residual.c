/* Regression-style reproducer for the low-surrogate misclassification
 * defect in *_toUtf16(), modeled after the project's own
 * test_misc_low_surrogate_mozilla_bug_2053153 test, but using the
 * surrogate pair for U+10000 (0xD800 0xDC00) instead of U+1F600.
 *
 * Usage: ./residual <n> <chunksize>
 *   n:         number of single "a" chars appended to the internal entity
 *              before the surrogate pair (try a range around 1024)
 *   chunksize: how many bytes are handed to XML_Parse() per call (0 means
 *              hand each whole piece to XML_Parse() in one call)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expat.h"

static int
feed(XML_Parser parser, const char *s, int len, int isFinal, int chunksize) {
  if (chunksize > 0) {
    for (; len > chunksize; len -= chunksize, s += chunksize) {
      enum XML_Status res = XML_Parse(parser, s, chunksize, 0);
      if (res == XML_STATUS_ERROR) {
        fprintf(stderr, "parse error: %s\n",
                XML_ErrorString(XML_GetErrorCode(parser)));
        return 0;
      }
    }
  }
  if (XML_Parse(parser, s, len, isFinal) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    return 0;
  }
  return 1;
}

int
main(int argc, char **argv) {
  size_t n = (argc > 1) ? (size_t)atoi(argv[1]) : 1023;
  int chunksize = (argc > 2) ? atoi(argv[2]) : 1;

  static const char doc_before[] =
      "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n\0"
      " \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'\0";
  static const char doc_after[] =
      "'\0>\0\n\0]\0>\0\n\0"
      "<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n\0";
  static const unsigned char pair_bytes[4] = {0x00, 0xd8, 0x00, 0xdc};

  XML_Parser parser = XML_ParserCreate(NULL);
  size_t i;

  if (!feed(parser, doc_before, (int)sizeof(doc_before) - 1, 0, chunksize))
    return 1;

  for (i = 0; i < n; i++) {
    if (!feed(parser, "a\0", 2, 0, chunksize))
      return 1;
  }

  if (!feed(parser, (const char *)pair_bytes, 4, 0, chunksize))
    return 1;

  if (!feed(parser, doc_after, (int)sizeof(doc_after) - 1, 1, chunksize))
    return 1;

  XML_ParserFree(parser);
  printf("done (n=%zu, chunksize=%d)\n", n, chunksize);
  return 0;
}
