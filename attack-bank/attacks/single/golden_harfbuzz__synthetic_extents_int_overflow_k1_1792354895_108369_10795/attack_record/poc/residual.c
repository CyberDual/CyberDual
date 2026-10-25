#include <stdint.h>

#include "hb.h"

static hb_bool_t
extents_func (hb_font_t *font, void *font_data,
              hb_codepoint_t glyph, hb_glyph_extents_t *extents,
              void *user_data)
{
  (void) font;
  (void) font_data;
  (void) glyph;
  (void) user_data;

  extents->x_bearing = INT32_MAX;
  extents->y_bearing = INT32_MAX;
  extents->width = 100;
  extents->height = 100;
  return 1;
}

int
main (void)
{
  hb_font_funcs_t *font_funcs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (font_funcs, extents_func, NULL, NULL);

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_funcs (font, font_funcs, NULL, NULL);
  hb_font_set_scale (font, 2048, 2048);
  hb_font_set_synthetic_bold (font, 0.05f, 0.05f, 1);

  hb_glyph_extents_t extents;
  (void) hb_font_get_glyph_extents (font, 1, &extents);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_font_funcs_destroy (font_funcs);
  return 0;
}
