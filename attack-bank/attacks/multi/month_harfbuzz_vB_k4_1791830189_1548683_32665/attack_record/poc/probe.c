#include "hb.h"
#include "hb-raster.h"

int main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw) return 1;

  hb_raster_extents_t extents = {0, 0, 20, 1, 0};
  hb_raster_draw_set_extents (draw, &extents);
  hb_draw_funcs_t *funcs = hb_raster_draw_get_funcs (draw);

  for (unsigned i = 0; i < 200; i++)
  {
    hb_draw_state_t state = HB_DRAW_STATE_DEFAULT;
    hb_draw_move_to (funcs, draw, &state, -0.25f, 0.f);
    hb_draw_line_to (funcs, draw, &state, 20.25f, 0.5f);
    hb_draw_line_to (funcs, draw, &state, -0.25f, 1.f);
    hb_draw_close_path (funcs, draw, &state);
  }

  hb_raster_image_t *image = hb_raster_draw_render (draw);
  if (!image) { hb_raster_draw_destroy (draw); return 1; }
  hb_raster_image_destroy (image);
  hb_raster_draw_destroy (draw);
  return 0;
}
