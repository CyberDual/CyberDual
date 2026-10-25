/*
 * Signed integer overflow in hb_font_t::synthetic_glyph_extents()
 * (embolden branch), reached from the public hb_font_get_glyph_extents().
 *
 * Font funcs may legitimately report extents whose fields were each
 * clamped independently to the full hb_position_t range: the glyf
 * producer's contour_bounds_t and the sbix png-dimension path both use
 * hb_clamp_to<hb_position_t>() per field, so a single field can be
 * INT32_MAX on its own.  With synthetic bold enabled on top of such
 * extents, the embolden shifts are still added in plain int32
 * arithmetic and extents->y_bearing overflows.
 *
 * Expected output on a fixed tree (values saturated):
 *   x_bearing=2147483596 y_bearing=2147483647 width=202 height=-2
 */
#include <stdio.h>
#include "hb.h"

static hb_bool_t
extreme_extents_func (hb_font_t *font, void *font_data,
		      hb_codepoint_t glyph,
		      hb_glyph_extents_t *extents,
		      void *user_data)
{
  (void) font; (void) font_data; (void) glyph; (void) user_data;
  extents->x_bearing = 2147483647; /* INT32_MAX */
  extents->y_bearing = 2147483647;
  extents->width = 100;
  extents->height = 100;
  return 1;
}

int
main (void)
{
  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (ffuncs, extreme_extents_func, NULL, NULL);

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 2048, 2048);

  /* Strengths become roundf(2048 * 0.05) = 102 scaled units. */
  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, 1);

  hb_glyph_extents_t extents;
  if (!hb_font_get_glyph_extents (font, 1, &extents))
  {
    fprintf (stderr, "hb_font_get_glyph_extents failed\n");
    return 1;
  }

  printf ("x_bearing=%d y_bearing=%d width=%d height=%d\n",
	  extents.x_bearing, extents.y_bearing,
	  extents.width, extents.height);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_font_funcs_destroy (ffuncs);
  return 0;
}
