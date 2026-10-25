#include <expat.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int
parse_single_bytes(XML_Parser parser, const char *bytes, size_t length,
                   int is_final) {
  for (size_t index = 0; index < length; ++index) {
    if (XML_Parse(parser, bytes + index, 1, 0) == XML_STATUS_ERROR)
      return 0;
  }

  if (is_final)
    return XML_Parse(parser, NULL, 0, 1) != XML_STATUS_ERROR;

  return 1;
}

int
main(void) {
  static const char doc_before[] = "<\0!\0D\0O\0C\0T\0Y\0P\0E\0 \0d\0 \0[\0\n\0"
                                   " \0 \0<\0!\0E\0N\0T\0I\0T\0Y\0 \0e\0 \0'\0";
  static const char doc_after[] = "'\0>\0\n\0]\0>\0\n\0"
                                  "<\0r\0 \0a\0=\0'\0&\0e\0;\0'\0/\0>\0\n\0";
  const size_t before_length = sizeof(doc_before) - 1 + 2 * 1023 + 4;
  char *const before = malloc(before_length);
  char *const after = malloc(sizeof(doc_after) - 1);

  if (before == NULL || after == NULL)
    return 2;

  memcpy(before, doc_before, sizeof(doc_before) - 1);
  for (size_t index = 0; index < 1023; ++index)
    memcpy(before + sizeof(doc_before) - 1 + 2 * index, "a\0", 2);
  memcpy(before + sizeof(doc_before) - 1 + 2 * 1023, "\x3d\xd8\x00\xdc", 4);
  memcpy(after, doc_after, sizeof(doc_after) - 1);

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  int ok = parse_single_bytes(parser, before, before_length, 0);
  ok = ok && parse_single_bytes(parser, after, sizeof(doc_after) - 1, 1);

  XML_ParserFree(parser);
  free(before);
  free(after);
  return ok ? 0 : 1;
}
