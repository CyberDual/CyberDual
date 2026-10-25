#include <hb.h>
#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw)
    return 0;

  hb_raster_extents_t ext = {0, 0, 1, 1, 0};
  hb_raster_draw_set_extents (draw, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (draw);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;
  hb_draw_move_to (df, draw, &st, 0.f, 0.f);
  hb_draw_line_to (df, draw, &st, 2.5f, 1.f);

  hb_raster_image_t *img = hb_raster_draw_render (draw);
  if (img)
    hb_raster_image_destroy (img);
  hb_raster_draw_destroy (draw);
  return 0;
}
