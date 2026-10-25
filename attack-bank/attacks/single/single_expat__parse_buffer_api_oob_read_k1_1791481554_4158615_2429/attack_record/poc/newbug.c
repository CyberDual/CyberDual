#include <string.h>
#include <stdio.h>
#include "expat.h"

/* Incremental in-contract use of the public buffer API:
   request a buffer of exactly N bytes, fill it with N bytes of a
   well-formed document, then hand those N bytes to the parser. */
int
main(void) {
  enum { N = 2048 };
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "could not create parser\n");
    return 2;
  }

  void *const buf = XML_GetBuffer(parser, N);
  if (buf == NULL) {
    fprintf(stderr, "XML_GetBuffer failed\n");
    XML_ParserFree(parser);
    return 2;
  }

  char *p = (char *)buf;
  memcpy(p, "<d>", 3);
  memset(p + 3, 'x', N - 7);
  memcpy(p + (N - 4), "</d>", 4);

  if (XML_ParseBuffer(parser, N, XML_TRUE) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  return 0;
}
