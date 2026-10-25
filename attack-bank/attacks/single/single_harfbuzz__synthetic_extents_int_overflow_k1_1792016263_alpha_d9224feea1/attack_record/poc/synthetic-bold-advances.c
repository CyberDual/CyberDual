/* Regression check for hb_font_get_glyph_h_advances with synthetic bold:
 * batch-advance retrieval over a caller-owned advance array. */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <hb.h>

static hb_position_t
glyph_h_advance (hb_font_t *font, void *font_data,
		 hb_codepoint_t glyph, void *user_data)
{
  (void) font, (void) font_data, (void) user_data;
  return 1000 + glyph % 23;
}

int
main (void)
{
  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_h_advance_func (ffuncs, glyph_h_advance, NULL, NULL);

  hb_font_t *font = hb_font_create (hb_face_get_empty ());
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 1000, 1000);

  /* Synthetic bold widens the advance of every non-space glyph. */
  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, false);

  hb_codepoint_t glyphs[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  unsigned int count = sizeof (glyphs) / sizeof (glyphs[0]);

  hb_position_t *advances = (hb_position_t *) malloc (count * sizeof (advances[0]));

  hb_font_get_glyph_h_advances (font, count,
				glyphs, sizeof (glyphs[0]),
				advances, sizeof (advances[0]));

  for (unsigned int i = 0; i < count; i++)
    printf ("advance[%u] = %d\n", i, advances[i]);

  free (advances);
  hb_font_destroy (font);
  hb_font_funcs_destroy (ffuncs);
  return 0;
}
