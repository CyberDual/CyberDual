#include "hb.h"
#include "hb-raster.h"

const char *
__asan_default_options (void)
{
  return "abort_on_error=1:detect_leaks=0";
}

int
main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw)
    return 1;

  hb_raster_extents_t extents =
  {
    .x_origin = 0,
    .y_origin = 0,
    .width = 8,
    .height = 1,
    .stride = 0,
  };
  hb_raster_draw_set_extents (draw, &extents);

  hb_draw_funcs_t *funcs = hb_raster_draw_get_funcs (draw);
  hb_draw_state_t state = HB_DRAW_STATE_DEFAULT;
  for (unsigned int i = 0; i < 63; i++)
  {
    hb_draw_move_to (funcs, draw, &state, 9.f, 0.f);
    hb_draw_line_to (funcs, draw, &state, 9.f, 1.f);
    hb_draw_close_path (funcs, draw, &state);
  }

  hb_draw_move_to (funcs, draw, &state, -1.f, 0.f);
  hb_draw_line_to (funcs, draw, &state, -1.f, 1.f);

  hb_raster_image_t *image = hb_raster_draw_render (draw);
  hb_raster_draw_destroy (draw);
  if (!image)
    return 1;

  hb_raster_image_destroy (image);
  return 0;
}
