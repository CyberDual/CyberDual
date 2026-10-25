#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
feed(XML_Parser parser, const char *bytes, size_t length, int final) {
  for (size_t i = 0; i < length; i++) {
    if (XML_Parse(parser, bytes + i, 1, final && i + 1 == length)
        != XML_STATUS_OK) {
      fprintf(stderr, "parse error: %d\n", XML_GetErrorCode(parser));
      return 0;
    }
  }
  return 1;
}

static int
feed_ascii(XML_Parser parser, const char *ascii, int final) {
  for (size_t i = 0, n = strlen(ascii); i < n; i++) {
    char unit[] = {0, ascii[i]};
    if (!feed(parser, unit, sizeof(unit), final && i + 1 == n)) return 0;
  }
  return 1;
}

int
main(void) {
  static const char pair[] = "\xd8\x3d\xde\x00";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) return 2;
  if (!feed_ascii(parser, "<!DOCTYPE r [\n  <!ENTITY e '", 0)) return 3;
  for (size_t i = 0; i < 1023; i++) {
    if (!feed_ascii(parser, "a", 0)) return 4;
  }
  if (!feed(parser, pair, sizeof(pair) - 1, 0)) return 5;
  if (!feed_ascii(parser, "'>\n]>\n<r a='&e;'/>\n", 1)) return 6;
  XML_ParserFree(parser);
  return 0;
}
