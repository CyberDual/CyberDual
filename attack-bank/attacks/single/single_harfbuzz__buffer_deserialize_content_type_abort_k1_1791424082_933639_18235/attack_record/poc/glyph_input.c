#include "hb.h"

#include <stdlib.h>
#include <string.h>

int
main (void)
{
  static const char glyphs[] = "[1=0]";
  char *input = (char *) malloc (sizeof (glyphs) - 1);
  if (!input)
    return 2;
  memcpy (input, glyphs, sizeof (glyphs) - 1);

  hb_buffer_t *buffer = hb_buffer_create ();
  const char *end;
  hb_bool_t parsed = hb_buffer_deserialize_glyphs (buffer, input,
                                                  sizeof (glyphs) - 1, &end,
                                                  NULL,
                                                  HB_BUFFER_SERIALIZE_FORMAT_TEXT);
  hb_buffer_destroy (buffer);
  free (input);
  return parsed ? 0 : 1;
}
