#include <expat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *
allocate_release_data(void) {
  return malloc(16);
}

static void XMLCALL
release_encoding_data(void *data) {
  free(data);
}

static int XMLCALL
supply_unknown_encoding(void *data, const XML_Char *name,
                        XML_Encoding *info) {
  (void)data;
  (void)name;

  for (int index = 0; index < 256; ++index)
    info->map[index] = index;
  info->data = allocate_release_data();
  info->convert = NULL;
  info->release = release_encoding_data;
  return info->data != NULL ? XML_STATUS_OK : XML_STATUS_ERROR;
}

int
main(void) {
  static const char document[]
      = "<?xml version='1.0' encoding='probe-encoding'?><root/>";
  XML_Parser parser = XML_ParserCreate(NULL);

  if (parser == NULL)
    return 1;

  XML_SetUnknownEncodingHandler(parser, supply_unknown_encoding, NULL);
  if (XML_Parse(parser, document, (int)strlen(document), XML_TRUE)
      != XML_STATUS_OK)
    return 1;

  XML_ParserReset(parser, NULL);
  XML_ParserFree(parser);
  return 0;
}
