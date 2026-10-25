/* Evaluation case probe: drives the public streaming API with a document
   that is in-contract for XML_Parse/XML_GetBuffer/XML_ParseBuffer usage. */
#include <stdio.h>
#include <string.h>
#include "expat.h"

/* Document prefix: an XML declaration whose version value keeps the "1."
   prefix, followed by an incomplete start tag "<r".  The rest of the
   document body is filled in through the buffer API below. */
static const char prefix[] = "<?xml version='1.x'?><r";

/* Size of the buffer that Expat allocates for the first document chunk. */
#define FIRST_BUFFER_SIZE (2 * 1024)

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  XML_SetReparseDeferralEnabled(parser, XML_FALSE);

  /* Parse the declaration plus the start of the root element; the "<r"
     stays unconsumed as a partial token. */
  XML_Parse(parser, prefix, (int)strlen(prefix), XML_FALSE);

  /* Fill the rest of the internal buffer with name characters and hand
     them over, so that the buffer is full but all in bounds. */
  {
    const int fill = FIRST_BUFFER_SIZE - (int)strlen(prefix);
    void *buf = XML_GetBuffer(parser, fill);
    if (buf == NULL) {
      XML_ParserFree(parser);
      return 0;
    }
    memset(buf, 'a', fill);
    XML_ParseBuffer(parser, fill, XML_FALSE);
  }

  /* Hand over two more bytes than the buffer can hold. */
  XML_ParseBuffer(parser, 2, XML_FALSE);

  XML_ParserFree(parser);
  return 0;
}
