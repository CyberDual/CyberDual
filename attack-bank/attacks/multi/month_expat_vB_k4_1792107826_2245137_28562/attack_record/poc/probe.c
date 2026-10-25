/* Drives the public libexpat API with ordinary, in-contract input.
 *
 * The document carries 25 attributes on one element (the library starts
 * with room for 16, so parsing must grow the attribute array) and a
 * 3000-character attribute value (the string pool starts with 1024-byte
 * blocks, so normalization must grow a pool block).  Both growths go
 * through the library's own reallocation path.
 */
#include <expat.h>

#include <stdio.h>
#include <string.h>

int
main(void) {
  static char doc[8192];
  size_t len = 0;
  int i;

  len += (size_t)sprintf(doc + len, "<root a='");
  for (i = 0; i < 3000; i++)
    doc[len++] = 'x';
  len += (size_t)sprintf(doc + len, "'");
  for (i = 0; i < 24; i++)
    len += (size_t)sprintf(doc + len, " b%02d='v'", i);
  len += (size_t)sprintf(doc + len, ">text</root>");

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }

  if (XML_Parse(parser, doc, (int)len, XML_TRUE) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  printf("ok\n");
  return 0;
}
