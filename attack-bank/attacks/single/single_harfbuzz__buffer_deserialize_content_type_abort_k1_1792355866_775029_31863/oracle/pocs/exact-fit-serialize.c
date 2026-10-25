#include <hb.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Serializes a one-item Unicode buffer with the "text" format into a
 * caller-provided buffer that is exactly as large as the serialized
 * output.  hb_buffer_serialize_unicode() must never write past
 * buf_size bytes.
 */
int
main (void)
{
  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, "A", -1, 0, -1);
  unsigned int count = hb_buffer_get_length (buffer);

  /* Determine the exact length of the serialized output. */
  char probe[64];
  unsigned int len = 0;
  hb_buffer_serialize_unicode (buffer, 0, count,
                               probe, sizeof probe, &len,
                               HB_BUFFER_SERIALIZE_FORMAT_TEXT,
                               HB_BUFFER_SERIALIZE_FLAG_DEFAULT);

  /* Serialize again into a buffer of exactly that size. */
  char *buf = (char *) malloc (len);
  unsigned int consumed = 0;
  hb_buffer_serialize_unicode (buffer, 0, count,
                               buf, len, &consumed,
                               HB_BUFFER_SERIALIZE_FORMAT_TEXT,
                               HB_BUFFER_SERIALIZE_FLAG_DEFAULT);

  printf ("serialized %u bytes: %s\n", consumed, buf);

  free (buf);
  hb_buffer_destroy (buffer);
  return 0;
}
