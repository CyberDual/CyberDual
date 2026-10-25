#include "expat.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  const size_t count = 120;
  const size_t name_len = 2048;
  const size_t capacity = count * (name_len + 40) + 100;
  char *xml = malloc(capacity);
  if (!xml)
    return 2;
  size_t used = (size_t)sprintf(xml, "<!DOCTYPE r [<!ENTITY a 'x'>");
  for (size_t i = 0; i < count; i++) {
    used += (size_t)sprintf(xml + used, "<!ENTITY a");
    memset(xml + used, 'b', name_len - 5);
    used += name_len - 5;
    used += (size_t)sprintf(xml + used, "%04zu 'x'>", i);
  }
  used += (size_t)sprintf(xml + used, "]><r/>");
  XML_Parser parser = XML_ParserCreate(NULL);
  if (!parser) {
    free(xml);
    return 2;
  }
  const uint8_t salt[16] = {0};
  XML_SetHashSalt16Bytes(parser, salt);
  enum XML_Status status = XML_Parse(parser, xml, (int)used, XML_TRUE);
  XML_ParserFree(parser);
  free(xml);
  return status == XML_STATUS_OK ? 0 : 1;
}
