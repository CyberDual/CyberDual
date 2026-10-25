/* PoC: hb_buffer_deserialize_glyphs() with an explicitly-sized buffer.
 *
 * Modeled on the project's own caller (src/test-buffer-serialize.cc),
 * which passes the input length explicitly rather than as -1.  The
 * buffer below is sized exactly to its content and is not
 * nul-terminated, which hb_buffer_deserialize_glyphs() documents as
 * legal ("the size of @buf").
 *
 * Build with -fsanitize=address and run: aborts on the patched tree
 * with a heap-buffer-overflow READ of size 1; runs clean on the parent.
 */
#include <stdlib.h>
#include <string.h>
#include <hb.h>

int main (void)
{
  hb_buffer_t *buffer = hb_buffer_create ();

  const char text[] = "[5=0]";
  size_t len = sizeof (text) - 1;

  char *data = (char *) malloc (len);
  memcpy (data, text, len);

  hb_buffer_deserialize_glyphs (buffer, data, (int) len, NULL, NULL,
				HB_BUFFER_SERIALIZE_FORMAT_TEXT);

  hb_buffer_destroy (buffer);
  free (data);
  return 0;
}
