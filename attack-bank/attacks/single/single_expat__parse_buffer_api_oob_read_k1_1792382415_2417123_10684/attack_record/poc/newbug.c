#include <expat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Feeds a document in three steps through XML_GetBuffer/XML_ParseBuffer:
 *
 *   1) "<doc><!--aaa..." (2000 bytes, non-final): the start tag is consumed,
 *      the unfinished comment stays buffered as a partial token.
 *   2) 48 more bytes closing the comment, still within the capacity that
 *      XML_GetBuffer reserved.
 *   3) A final XML_ParseBuffer whose length overshoots the reserved
 *      capacity while the partial token is still pending.
 */
int main(void) {
  XML_Parser p = XML_ParserCreate(NULL);
  if (! p)
    return 2;

  /* Step 1: 2000 bytes into a fresh buffer (capacity 2048). */
  char *buf = XML_GetBuffer(p, 2000);
  if (! buf)
    return 3;
  memcpy(buf, "<doc><!--", 9);
  memset(buf + 9, 'a', 2000 - 9);
  if (XML_ParseBuffer(p, 2000, XML_FALSE) == XML_STATUS_ERROR) {
    printf("step1 error=%d\n", (int)XML_GetErrorCode(p));
    return 4;
  }

  /* Step 2: finish the comment within the reserved capacity. */
  buf = XML_GetBuffer(p, 48);
  if (! buf)
    return 5;
  memcpy(buf, "-->", 3);
  buf[3] = '\x01';
  memset(buf + 4, 'b', 44);

  /* Step 3: claim more than the 48 reserved bytes while a part of the
     input is still pending in the buffer. */
  enum XML_Status st = XML_ParseBuffer(p, 148, XML_TRUE);
  printf("status=%d error=%d\n", (int)st, (int)XML_GetErrorCode(p));
  XML_ParserFree(p);
  return 0;
}
