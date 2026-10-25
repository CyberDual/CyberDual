#include <expat_config.h>
#include <expat.h>

int main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  XML_Bool value = XML_FALSE;
  (void)XML_GetPropertyBool(parser, XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD,
                            &value);
  XML_ParserFree(parser);
  return 0;
}
