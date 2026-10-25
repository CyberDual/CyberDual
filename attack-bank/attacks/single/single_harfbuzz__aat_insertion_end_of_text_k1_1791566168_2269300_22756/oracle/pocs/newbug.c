/* Self-contained PoC for the morx end-of-text insertion defect.
 *
 * Builds a tiny AAT (morx) font whose insertion state machine fires a
 * current-insertion action at end-of-text.  On the patched tree the
 * end-of-text fixup loop in InsertionSubtable::transition reads one past
 * the sanitized insertion-glyph array (glyphs[count]); because the
 * insertion-glyph table is the last data in the font, that read lands
 * past the blob and AddressSanitizer aborts.  The parent tree has no such
 * read and shapes the string normally.
 *
 * Public-API entry point only: hb_blob_create / hb_face_create /
 * hb_font_create / hb_buffer_* / hb_shape.
 */
#include <stdio.h>
#include "hb.h"

static const unsigned char font_data[] = {
  0,1,0,0,0,6,0,64,0,2,0,32,99,109,97,112,
  0,12,0,116,0,0,0,108,0,0,0,44,104,101,97,100,
  98,251,68,205,0,0,0,152,0,0,0,54,104,104,101,97,
  7,10,3,35,0,0,0,208,0,0,0,36,104,109,116,120,
  7,208,0,0,0,0,0,244,0,0,0,8,109,97,120,112,
  0,2,80,0,0,0,0,252,0,0,0,6,109,111,114,120,
  0,40,1,106,0,0,1,4,0,0,0,108,0,0,0,1,
  0,3,0,1,0,0,0,12,0,4,0,32,0,0,0,4,
  0,4,0,1,0,0,0,65,255,255,0,0,0,65,255,255,
  255,192,0,1,0,0,0,0,0,1,0,0,0,1,0,0,
  0,0,0,0,95,15,60,245,0,0,3,232,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  3,232,3,232,0,0,0,8,0,2,0,0,0,0,0,0,
  0,1,0,0,3,32,255,56,0,0,3,232,0,0,0,0,
  3,232,0,1,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,2,3,232,0,0,3,232,0,0,0,0,80,0,
  0,2,0,0,0,2,0,0,0,0,0,1,0,0,0,1,
  0,0,0,100,0,0,0,0,0,0,0,1,0,0,0,84,
  0,0,0,5,0,0,0,1,0,0,0,5,0,0,0,20,
  0,0,0,26,0,0,0,46,0,0,0,70,0,0,0,1,
  0,4,0,0,0,0,0,0,0,0,0,1,0,2,0,0,
  0,0,0,0,0,1,0,0,0,0,255,255,255,255,0,1,
  0,0,255,255,255,255,0,0,0,32,0,0,255,255,0,1,
};

int main (void)
{
  hb_blob_t *blob = hb_blob_create ((const char *) font_data, sizeof (font_data),
                                    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_font_t *font = hb_font_create (face);

  hb_buffer_t *buf = hb_buffer_create ();
  hb_buffer_add_utf8 (buf, "A", -1, 0, -1);
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
  return 0;
}
