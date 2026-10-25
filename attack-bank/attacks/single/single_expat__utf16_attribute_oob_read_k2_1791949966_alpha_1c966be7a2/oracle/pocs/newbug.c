#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UTF-16LE document: <r a="AAA...U+1F600"></r>
 * U+1F600 is the surrogate pair D83D DE00. 1023 BMP units then that pair
 * is still a well-formed XML attribute value. */

enum { BMP = 1023 };

static unsigned char *
build(int *len_out) {
  static const char head[] = "<r a=\"";
  static const char tail[] = "\"></r>";
  const int nunits = (int)(sizeof(head) - 1) + BMP + 2 + (int)(sizeof(tail) - 1);
  unsigned char *buf = (unsigned char *)malloc((size_t)nunits * 2);
  if (!buf)
    return NULL;
  int o = 0;
  for (int i = 0; head[i]; i++) {
    buf[o++] = (unsigned char)head[i];
    buf[o++] = 0;
  }
  for (int i = 0; i < BMP; i++) {
    buf[o++] = 'A';
    buf[o++] = 0;
  }
  buf[o++] = 0x3D;
  buf[o++] = 0xD8;
  buf[o++] = 0x00;
  buf[o++] = 0xDE;
  for (int i = 0; tail[i]; i++) {
    buf[o++] = (unsigned char)tail[i];
    buf[o++] = 0;
  }
  *len_out = o;
  return buf;
}

static void XMLCALL
on_start(void *ud, const XML_Char *name, const XML_Char **atts) {
  (void)ud;
  (void)name;
  if (!atts || !atts[0] || !atts[1])
    return;
  volatile unsigned acc = 0;
  for (const XML_Char *p = atts[1]; *p; p++)
    acc += *p;
  (void)acc;
}

int
main(void) {
  static const XML_Char enc[] = {'U', 'T', 'F', '-', '1', '6', 'L', 'E', 0};
  int len = 0;
  unsigned char *doc = build(&len);
  if (!doc)
    return 2;
  XML_Parser parser = XML_ParserCreate(enc);
  if (!parser) {
    free(doc);
    return 2;
  }
  XML_SetStartElementHandler(parser, on_start);
  const enum XML_Status st = XML_Parse(parser, (const char *)doc, len, 1);
  const int rc = st == XML_STATUS_OK ? 0 : 1;
  XML_ParserFree(parser);
  free(doc);
  return rc;
}
