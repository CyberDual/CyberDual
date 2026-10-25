#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat.h"

/* Parse a document larger than the default context window and ask for the
   input context through the public location API. */
int
main(void) {
  const size_t body = 1400;
  const char *prefix = "<doc>";
  const char *suffix = "</doc>";
  const size_t prefix_len = strlen(prefix);
  const size_t suffix_len = strlen(suffix);
  const size_t len = prefix_len + body + suffix_len;
  char *doc = (char *)malloc(len);
  XML_Parser parser;
  int offset = -1;
  int size = -1;
  const char *context;
  volatile unsigned char acc = 0;
  int i;

  if (doc == NULL)
    return 1;
  memcpy(doc, prefix, prefix_len);
  memset(doc + prefix_len, 'a', body);
  memcpy(doc + prefix_len + body, suffix, suffix_len);

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    free(doc);
    return 1;
  }
  if (XML_Parse(parser, doc, (int)len, XML_TRUE) != XML_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    free(doc);
    return 1;
  }

  context = XML_GetInputContext(parser, &offset, &size);
  if (context == NULL || size < 0 || offset < 0 || offset > size) {
    fprintf(stderr, "context unavailable\n");
    XML_ParserFree(parser);
    free(doc);
    return 2;
  }
  for (i = 0; i < size; i++)
    acc = (unsigned char)(acc + (unsigned char)context[i]);

  printf("ok offset=%d size=%d acc=%u\n", offset, size, (unsigned)acc);
  XML_ParserFree(parser);
  free(doc);
  return 0;
}
