#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  hb_raster_extents_t ext = {0, 0, 8, 1, 0};
  hb_draw_funcs_t *df;
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;
  hb_raster_image_t *img;

  if (!draw)
    return 0;

  hb_raster_draw_set_extents (draw, &ext);
  df = hb_raster_draw_get_funcs (draw);

  hb_draw_move_to (df, draw, &st, 1.25f, 0.25f);
  hb_draw_line_to (df, draw, &st, 8.5f, 0.75f);

  hb_draw_move_to (df, draw, &st, 9.5f, 0.20f);
  hb_draw_line_to (df, draw, &st, 8.5f, 0.80f);

  img = hb_raster_draw_render (draw);
  hb_raster_image_destroy (img);
  hb_raster_draw_destroy (draw);
  return 0;
}
