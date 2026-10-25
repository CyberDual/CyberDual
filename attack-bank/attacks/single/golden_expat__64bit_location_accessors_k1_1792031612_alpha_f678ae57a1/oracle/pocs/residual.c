/* In-contract parse of a modest document that asks for the 64-bit input
   context from the start-element handler. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat.h"

static void XMLCALL
on_start(void *userData, const XML_Char *name, const XML_Char **atts) {
  XML_Parser parser = (XML_Parser)userData;
  int64_t offset = -1;
  uint64_t size = 0;
  const char *ctx;

  (void)name;
  (void)atts;

  ctx = XML_GetInputContext64(parser, &offset, &size);
  if (ctx == NULL) {
    fprintf(stderr, "XML_GetInputContext64 returned NULL\n");
    exit(2);
  }
  /* Walk the buffer the API says is valid. */
  {
    volatile unsigned char acc = 0;
    uint64_t i;
    for (i = 0; i < size && i < 64; i++)
      acc = (unsigned char)(acc + (unsigned char)ctx[i]);
    (void)acc;
  }
  printf("offset=%lld size=%llu\n", (long long)offset, (unsigned long long)size);
}

int
main(void) {
  enum XML_Status status;
  XML_Parser parser;
  const int doc_len = 1500;
  char *doc = (char *)malloc((size_t)doc_len + 1);
  if (doc == NULL)
    return 1;

  memcpy(doc, "<r>", 3);
  memset(doc + 3, 'a', (size_t)doc_len - 3 - 4);
  memcpy(doc + doc_len - 4, "</r>", 4);
  doc[doc_len] = '\0';

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  XML_SetUserData(parser, parser);
  XML_SetStartElementHandler(parser, on_start);

  status = XML_Parse(parser, doc, doc_len, XML_TRUE);
  printf("status=%d\n", (int)status);
  XML_ParserFree(parser);
  free(doc);
  return status == XML_STATUS_OK ? 0 : 1;
}
