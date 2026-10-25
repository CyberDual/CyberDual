#include <hb.h>
#include <stdio.h>

static hb_bool_t
get_glyph_extents (hb_font_t *font,
		   void *font_data,
		   hb_codepoint_t glyph,
		   hb_glyph_extents_t *extents,
		   void *user_data)
{
  (void) font;
  (void) font_data;
  (void) glyph;
  (void) user_data;

  extents->x_bearing = 50;
  extents->y_bearing = 400;
  extents->width = 600;
  extents->height = 700;
  return 1;
}

int
main (void)
{
  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);

  hb_font_funcs_t *funcs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (funcs, get_glyph_extents, NULL, NULL);
  hb_font_set_funcs (font, funcs, NULL, NULL);
  hb_font_funcs_destroy (funcs);

  hb_font_set_scale (font, 2048, 2048);
  hb_font_set_synthetic_slant (font, -0.2f);

  hb_glyph_extents_t extents;
  if (hb_font_get_glyph_extents (font, 3, &extents))
    printf ("%d %d %d %d\n",
	    extents.x_bearing, extents.y_bearing,
	    extents.width, extents.height);

  hb_font_destroy (font);
  hb_face_destroy (face);
  return 0;
}
