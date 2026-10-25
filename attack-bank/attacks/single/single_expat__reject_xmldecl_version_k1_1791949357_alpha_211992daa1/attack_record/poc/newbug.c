#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat.h"

/* Document with a legal VersionNum ('1.' and an unbounded digit run). */
int
main(void) {
  enum { kVersionChars = 128 };
  static const char pre[] = "<?xml version=\"";
  static const char post[] = "\"?><doc/>";
  const size_t pre_n = sizeof(pre) - 1;
  const size_t post_n = sizeof(post) - 1;
  const size_t len = pre_n + (size_t)kVersionChars + post_n;
  char *doc;
  XML_Parser parser;
  enum XML_Status status;
  int i;

  doc = (char *)malloc(len + 1);
  if (doc == NULL)
    return 2;
  memcpy(doc, pre, pre_n);
  doc[pre_n] = '1';
  doc[pre_n + 1] = '.';
  for (i = 2; i < kVersionChars; i++)
    doc[pre_n + (size_t)i] = '0';
  memcpy(doc + pre_n + (size_t)kVersionChars, post, post_n + 1);

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    free(doc);
    return 2;
  }
  status = XML_Parse(parser, doc, (int)len, XML_TRUE);
  if (status == XML_STATUS_ERROR) {
    fprintf(stderr, "%s\n", XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    free(doc);
    return 1;
  }
  XML_ParserFree(parser);
  free(doc);
  return 0;
}
