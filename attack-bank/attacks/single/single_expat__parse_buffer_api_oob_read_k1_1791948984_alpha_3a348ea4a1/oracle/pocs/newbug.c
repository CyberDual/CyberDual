#include <stdio.h>
#include <string.h>

#include "expat.h"

/* Fill the buffer XML_GetBuffer just returned and parse that many bytes. */
int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  /* 2048 is the size XML_GetBuffer actually allocates for this request
     (INIT_BUFFER_SIZE is 1024 and the buffer is doubled until it fits). */
  enum { N = 2048 };
  char *buf = XML_GetBuffer(parser, N);
  if (buf == NULL) {
    XML_ParserFree(parser);
    return 2;
  }

  memcpy(buf, "<a>", 3);
  memset(buf + 3, 'x', (size_t)N - 7);
  memcpy(buf + (N - 4), "</a>", 4);

  const enum XML_Status status = XML_ParseBuffer(parser, N, XML_TRUE);
  XML_ParserFree(parser);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "parse status %d\n", (int)status);
    return 1;
  }
  return 0;
}
