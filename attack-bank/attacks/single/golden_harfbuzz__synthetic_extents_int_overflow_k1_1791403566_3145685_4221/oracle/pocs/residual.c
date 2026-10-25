#include <hb.h>
#include <stdio.h>
#include <limits.h>

static hb_bool_t
extreme_extents_func (hb_font_t *font, void *font_data,
                      hb_codepoint_t glyph,
                      hb_glyph_extents_t *extents,
                      void *user_data)
{
  (void) font; (void) font_data; (void) glyph; (void) user_data;
  extents->x_bearing = 100;
  extents->y_bearing = INT_MAX;
  extents->width = 100;
  extents->height = 100;
  return 1;
}

int main (void)
{
  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (ffuncs, extreme_extents_func, NULL, NULL);

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_set_scale (font, 2048, 2048);

  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, 1);

  hb_glyph_extents_t extents;
  hb_bool_t ok = hb_font_get_glyph_extents (font, 1, &extents);
  printf ("ok=%d x=%d y=%d w=%d h=%d\n", ok, extents.x_bearing, extents.y_bearing, extents.width, extents.height);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_font_funcs_destroy (ffuncs);
  return 0;
}
