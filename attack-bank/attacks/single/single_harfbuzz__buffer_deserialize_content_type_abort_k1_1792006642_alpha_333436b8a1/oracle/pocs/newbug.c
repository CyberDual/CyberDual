#include <hb.h>
#include <stdio.h>

int
main (void)
{
  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_serialize_format_t format =
    hb_buffer_serialize_format_from_string ("xml", -1);
  const char *end_ptr = NULL;

  hb_bool_t parsed = hb_buffer_deserialize_glyphs (buffer, "[1|2|3]", -1,
						   &end_ptr, NULL, format);

  printf ("parsed: %d\n", parsed);

  hb_buffer_destroy (buffer);
  return 0;
}
