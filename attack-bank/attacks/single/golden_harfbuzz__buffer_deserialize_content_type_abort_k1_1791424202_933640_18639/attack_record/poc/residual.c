#include "hb.h"

int main (void)
{
  hb_buffer_t *buffer = hb_buffer_create ();
  const char input[] = "A";
  const char *end;
  hb_bool_t result = hb_buffer_deserialize_unicode (buffer, input, 1, &end,
                                                    HB_BUFFER_SERIALIZE_FORMAT_INVALID);
  hb_buffer_destroy (buffer);
  return result != 0;
}
