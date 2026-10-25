#include "expat.h"
#include <stdlib.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (! parser)
    return 1;

  XML_Bool *enabled = malloc(sizeof(*enabled));
  if (! enabled) {
    XML_ParserFree(parser);
    return 1;
  }

  enum XML_Prop_Error error
      = XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED, enabled);
  free(enabled);
  XML_ParserFree(parser);
  return error == XML_PROP_ERROR_NONE ? 0 : 1;
}
