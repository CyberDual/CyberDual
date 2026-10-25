/* Drives hb_font_get_glyph_extents with synthetic bold enabled and a
 * glyph-extents func that reports an extreme width, which the extent
 * producers can legitimately emit after clamping each field to the full
 * hb_position_t range independently. */
#include <hb.h>
#include <limits.h>
#include <stdio.h>

static hb_bool_t
extreme_glyph_extents_func (hb_font_t *font, void *font_data,
			    hb_codepoint_t glyph,
			    hb_glyph_extents_t *extents,
			    void *user_data)
{
  (void) font; (void) font_data; (void) glyph; (void) user_data;
  extents->x_bearing = 0;
  extents->y_bearing = 0;
  extents->width = INT_MAX;
  extents->height = 100;
  return 1;
}

int
main (void)
{
  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (ffuncs, extreme_glyph_extents_func, NULL, NULL);

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 2048, 2048);
  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, 1);

  hb_glyph_extents_t extents;
  hb_bool_t ret = hb_font_get_glyph_extents (font, 1, &extents);
  printf ("ret=%d x_bearing=%d y_bearing=%d width=%d height=%d\n",
	  (int) ret, extents.x_bearing, extents.y_bearing,
	  extents.width, extents.height);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_font_funcs_destroy (ffuncs);
  return 0;
}
