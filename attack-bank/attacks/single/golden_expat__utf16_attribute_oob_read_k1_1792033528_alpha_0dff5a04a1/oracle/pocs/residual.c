#include <stddef.h>
#include <stdio.h>

#include "expat.h"

/* UTF-16-LE document:
 *   <!DOCTYPE d [
 *     <!ENTITY e 'aaa...aU+10000'>
 *   ]>
 *   <r a='&e;'/>
 * U+10000 is a legal XML 1.0 character (UTF-16: D800 DC00).
 * XML_Parse is invoked on one byte at a time, which the API allows.
 */
static int
parse_bytewise(XML_Parser parser, const char *s, int len, int is_final) {
  while (len > 1) {
    if (XML_Parse(parser, s, 1, XML_FALSE) != XML_STATUS_OK)
      return 0;
    s++;
    len--;
  }
  return XML_Parse(parser, s, len, is_final) == XML_STATUS_OK;
}

int
main(void) {
  static const char before[] = "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n\0"
                                " \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'\0";
  static const char after[] = "'\0>\0\n\0]\0>\0\n\0"
                               "<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n\0";
  /* U+10000 in UTF-16-LE. */
  static const char pair[] = "\x00\xd8\x00\xdc";
  const char a_unit[] = {'a', '\0'};
  enum { N_A = 1023 };
  int i;

  XML_Parser parser = XML_ParserCreate(NULL);
  if (!parser)
    return 1;
  if (!parse_bytewise(parser, before, (int)(sizeof(before) - 1), XML_FALSE))
    return 2;
  for (i = 0; i < N_A; i++) {
    if (!parse_bytewise(parser, a_unit, 2, XML_FALSE))
      return 3;
  }
  if (!parse_bytewise(parser, pair, 4, XML_FALSE))
    return 4;
  if (!parse_bytewise(parser, after, (int)(sizeof(after) - 1), XML_TRUE))
    return 5;
  XML_ParserFree(parser);
  return 0;
}
