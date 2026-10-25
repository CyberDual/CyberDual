/* Drives the public XML_GetBuffer/XML_ParseBuffer API with a len that is
 * inconsistent with the buffer space actually obtained: the first
 * XML_ParseBuffer consumes the entire buffer, so the second call's len
 * exceeds the space left.  A hardened library must reject that call with
 * XML_ERROR_INVALID_ARGUMENT; a vulnerable one reads past the buffer. */
#include <stdio.h>
#include <string.h>

#include <expat.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  /* Obtain a buffer and fill it completely with a valid partial document */
  char *const buf = (char *)XML_GetBuffer(parser, 2048);
  if (buf == NULL)
    return 2;
  memset(buf, 'A', 2048);
  memcpy(buf, "<doc>", 5);

  /* Consume the whole buffer: start tag followed by character data */
  if (XML_ParseBuffer(parser, 2048, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "setup parse unexpectedly failed: error %d\n",
            (int)XML_GetErrorCode(parser));
    XML_ParserFree(parser);
    return 2;
  }

  /* Zero bytes of space remain in the buffer, so this len is inconsistent
   * with what XML_GetBuffer provided. */
  if (XML_ParseBuffer(parser, 2048, XML_FALSE) == XML_STATUS_ERROR
      && XML_GetErrorCode(parser) == XML_ERROR_INVALID_ARGUMENT) {
    puts("rejected: library not vulnerable");
    XML_ParserFree(parser);
    return 0;
  }

  XML_ParserFree(parser);
  puts("inconsistent len accepted");
  return 1;
}
