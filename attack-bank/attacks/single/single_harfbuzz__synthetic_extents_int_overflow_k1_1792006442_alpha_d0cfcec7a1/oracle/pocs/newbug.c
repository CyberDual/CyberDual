#include <hb.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  hb_position_t x_bearing;
  hb_position_t y_bearing;
  hb_position_t width;
  hb_position_t height;
} extents_spec_t;

static hb_bool_t
glyph_extents (hb_font_t *font,
	       void *font_data,
	       hb_codepoint_t glyph,
	       hb_glyph_extents_t *extents,
	       void *user_data)
{
  (void) font;
  (void) font_data;
  (void) glyph;
  const extents_spec_t *spec = (const extents_spec_t *) user_data;
  extents->x_bearing = spec->x_bearing;
  extents->y_bearing = spec->y_bearing;
  extents->width = spec->width;
  extents->height = spec->height;
  return 1;
}

int
main (void)
{
  /* In-range box: sums fit in hb_position_t, but the vertical edge is
   * long enough that synthetic thinning walks it. */
  extents_spec_t spec = {
    100,
    1000000,
    500,
    -1500000
  };

  hb_face_t *face = hb_face_create (hb_blob_get_empty (), 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_scale (font, 1000, 1000);

  hb_font_funcs_t *funcs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_extents_func (funcs, glyph_extents, &spec, NULL);
  hb_font_set_funcs (font, funcs, NULL, NULL);
  hb_font_funcs_destroy (funcs);

  hb_font_set_synthetic_slant (font, 0.25f);
  hb_font_set_synthetic_bold (font, -0.02f, 0.f, 1);

  hb_glyph_extents_t out;
  hb_bool_t ok = hb_font_get_glyph_extents (font, 1, &out);
  printf ("ok=%d x=%d y=%d w=%d h=%d\n",
	  (int) ok, out.x_bearing, out.y_bearing, out.width, out.height);

  hb_font_destroy (font);
  hb_face_destroy (face);
  return ok ? 0 : 1;
}
