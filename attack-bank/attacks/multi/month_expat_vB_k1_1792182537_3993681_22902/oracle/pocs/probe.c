#include <expat.h>
#include <stdio.h>
#include <string.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreateNS(NULL, '|');
  if (! parser)
    return 0;
  XML_SetReturnNSTriplet(parser, 1);

  char local[46];
  memset(local, 'L', 45);
  local[45] = '\0';

  char uri[111];
  memcpy(uri, "http://example.invalid/", 23);
  memset(uri + 23, 'u', 87);
  uri[110] = '\0';

  char doc[256];
  int n = snprintf(doc, sizeof(doc), "<a:%s xmlns:a=\"%s\">z</a:%s>", local, uri,
                   local);
  if (n < 0 || n >= (int)sizeof(doc))
    return 0;

  XML_Parse(parser, doc, n, 1);
  XML_ParserFree(parser);
  return 0;
}
