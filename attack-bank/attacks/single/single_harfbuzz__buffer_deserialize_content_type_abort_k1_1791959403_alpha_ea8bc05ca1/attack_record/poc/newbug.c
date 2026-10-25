#include <hb.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{
  const char text[] = "<U+0041=0>";
  size_t n = sizeof (text) - 1;
  char *buf = (char *) malloc (n);
  if (!buf)
    return 1;
  memcpy (buf, text, n);

  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_deserialize_unicode (buffer, buf, (int) n, NULL,
                                 HB_BUFFER_SERIALIZE_FORMAT_TEXT);
  hb_buffer_destroy (buffer);
  free (buf);
  return 0;
}
