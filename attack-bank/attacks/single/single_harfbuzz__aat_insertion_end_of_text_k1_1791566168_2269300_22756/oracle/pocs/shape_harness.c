#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hb.h"

int main (int argc, char **argv)
{
  if (argc < 3) { fprintf (stderr, "usage: %s font.ttf text\n", argv[0]); return 2; }

  FILE *f = fopen (argv[1], "rb");
  if (!f) { fprintf (stderr, "cannot open %s\n", argv[1]); return 2; }
  fseek (f, 0, SEEK_END);
  long sz = ftell (f);
  fseek (f, 0, SEEK_SET);
  char *data = (char *) malloc (sz);           /* exact size: ASan tracks the end tightly */
  if (fread (data, 1, sz, f) != (size_t) sz) { fprintf (stderr, "read error\n"); return 2; }
  fclose (f);

  hb_blob_t *blob = hb_blob_create (data, (unsigned) sz, HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_font_t *font = hb_font_create (face);

  hb_buffer_t *buf = hb_buffer_create ();
  hb_buffer_add_utf8 (buf, argv[2], -1, 0, -1);
  hb_buffer_guess_segment_properties (buf);

  hb_shape (font, buf, NULL, 0);

  unsigned int count = 0;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buf, &count);
  printf ("glyphs=%u:", count);
  for (unsigned int i = 0; i < count; i++)
    printf (" %u", info[i].codepoint);
  printf ("\n");

  hb_buffer_destroy (buf);
  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_blob_destroy (blob);
  free (data);
  return 0;
}
