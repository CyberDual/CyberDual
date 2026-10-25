/*
 * Regression check for glyph extents with synthetic slant.
 *
 * Exercises hb_font_get_glyph_extents() on a font whose glyph-extents
 * callback reports an ordinary bounding box while synthetic slant is
 * enabled, driving the synthetic-extents fallback path end to end
 * through the public API.
 *
 * Copy, compile and run under AddressSanitizer:
 *
 *   cc -g -O1 -fsanitize=address -Isrc -DHB_NO_FEATURES_H \
 *      pocs/newbug.c src/harfbuzz.cc -lm -lstdc++ -o /tmp/newbug
 *   ASAN_OPTIONS=abort_on_error=1 /tmp/newbug
 */

#include <hb.h>
#include <stdio.h>

static hb_bool_t
glyph_extents_func (hb_font_t *font,
		    void *font_data,
		    hb_codepoint_t glyph,
		    hb_glyph_extents_t *extents,
		    void *user_data)
{
  (void) font;
  (void) font_data;
  (void) glyph;
  (void) user_data;

  extents->x_bearing = 100;
  extents->y_bearing = 500;
  extents->width = 300;
  extents->height = -600;
  return 1;
}

int
main (void)
{
  hb_face_t *face = hb_face_get_empty ();
  hb_font_t *font = hb_font_create (face);

  hb_font_funcs_t *funcs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (funcs, glyph_extents_func, NULL, NULL);
  hb_font_set_funcs (font, funcs, NULL, NULL);
  hb_font_funcs_destroy (funcs);

  hb_font_set_synthetic_slant (font, 0.2f);

  hb_glyph_extents_t extents;
  hb_bool_t ret = hb_font_get_glyph_extents (font, 1, &extents);
  printf ("ret=%d x=%d y=%d w=%d h=%d\n", ret,
	  extents.x_bearing, extents.y_bearing,
	  extents.width, extents.height);

  hb_font_destroy (font);
  return 0;
}
