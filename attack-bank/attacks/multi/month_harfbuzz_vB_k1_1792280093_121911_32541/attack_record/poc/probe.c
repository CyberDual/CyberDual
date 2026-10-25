/*
 * Probe: rasterize an outline whose edge enters the surface from the
 * right, through the public hb-raster.h draw API.
 *
 * The surface is pinned to a 10x8 window with hb_raster_draw_set_extents();
 * the outline then draws a right-to-left edge that starts four columns
 * right of the window and ends inside it.  The sweep must fold the
 * out-of-surface part of the walk into the visible window.
 *
 * Build: linked against an AddressSanitizer build of HarfBuzz.
 * Clean run: renders the 10x8 mask and exits 0.
 */

#include <hb.h>
#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *rdr = hb_raster_draw_create_or_fail ();
  if (!rdr)
    return 1;

  hb_raster_extents_t ext = {0, 0, 10, 8, 0};
  hb_raster_draw_set_extents (rdr, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (rdr);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  /* A triangle whose hypotenuse runs right-to-left, starting at x=14
   * (four columns past the 10px surface) and ending at x=2. */
  hb_draw_move_to (df, rdr, &st, 14.f, 0.f);
  hb_draw_line_to (df, rdr, &st, 2.f, 1.f);
  hb_draw_line_to (df, rdr, &st, 2.f, 0.f);
  hb_draw_close_path (df, rdr, &st);

  hb_raster_image_t *img = hb_raster_draw_render (rdr);
  if (!img)
    return 2;

  hb_raster_extents_t out;
  hb_raster_image_get_extents (img, &out);
  if (out.width != 10 || out.height != 8)
    return 3;

  hb_raster_image_destroy (img);
  hb_raster_draw_destroy (rdr);
  return 0;
}
