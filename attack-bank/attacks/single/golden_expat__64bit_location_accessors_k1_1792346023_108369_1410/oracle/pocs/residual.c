#include <expat.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
start_element(void *user_data, const XML_Char *name, const XML_Char **atts) {
  (void)user_data;
  (void)name;
  (void)atts;

  int64_t offset;
  uint64_t size;
  const char *const context = XML_GetInputContext64(user_data, &offset, &size);

  if (context == NULL) {
    fprintf(stderr, "unexpected missing input context\n");
    return;
  }

  volatile const char *const end = context + size - 1;
  (void)*end;
}

int
main(void) {
  char text[2048] = "<doc>";
  XML_Parser parser = XML_ParserCreate(NULL);

  XML_SetUserData(parser, parser);
  memset(text + strlen(text), 'x', sizeof(text) - strlen("<doc></doc>"));
  memcpy(text + sizeof(text) - strlen("</doc>"), "</doc>", strlen("</doc>"));
  XML_SetElementHandler(parser, start_element, NULL);

  if (XML_Parse(parser, text, (int)sizeof(text), XML_TRUE) != XML_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  return 0;
}
