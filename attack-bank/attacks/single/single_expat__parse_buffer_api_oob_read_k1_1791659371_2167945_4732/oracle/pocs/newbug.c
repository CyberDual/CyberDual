#include <expat.h>
#include <string.h>
#include <stdio.h>

int main(void) {
  XML_Parser p = XML_ParserCreate(NULL);

  void *buf = XML_GetBuffer(p, 16);
  memcpy(buf, "<a>", 3);
  enum XML_Status st0 = XML_ParseBuffer(p, 3, 0);
  printf("status0=%d\n", st0);

  void *buf2 = XML_GetBuffer(p, 16);
  memcpy(buf2, "x", 1);
  enum XML_Status st = XML_ParseBuffer(p, 100000, 0);
  printf("status=%d error=%s\n", st, XML_ErrorString(XML_GetErrorCode(p)));

  unsigned long line = (unsigned long)XML_GetCurrentLineNumber(p);
  printf("line=%lu\n", line);

  XML_ParserFree(p);
  return 0;
}
