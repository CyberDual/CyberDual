#include <hb.h>
#include <hb-raster.h>

/* Rasterize one diagonal that ends on the exclusive right edge of a
 * fixed surface.  In-contract public draw API: coordinates are pixel
 * space, extents cover the surface the caller asked for. */
int
main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw)
    return 0;

  hb_raster_extents_t ext = {
    0, 0,
    8, 4,
    0
  };
  hb_raster_draw_set_extents (draw, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (draw);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  hb_draw_move_to (df, draw, &st, 0.f, 0.f);
  hb_draw_line_to (df, draw, &st, 8.f, 4.f);
  hb_draw_line_to (df, draw, &st, 0.f, 4.f);
  hb_draw_close_path (df, draw, &st);

  hb_raster_image_t *img = hb_raster_draw_render (draw);
  if (img)
  {
    const uint8_t *px = hb_raster_image_get_buffer (img);
    if (px)
      (void) px[0];
    hb_raster_image_destroy (img);
  }
  hb_raster_draw_destroy (draw);
  return 0;
}
