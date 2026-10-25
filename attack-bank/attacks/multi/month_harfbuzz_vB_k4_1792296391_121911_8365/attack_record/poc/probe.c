/*
 * probe.c — cropped-outline raster regression probe.
 *
 * Rasterizes a small triangle whose long edge enters the output
 * surface from beyond its right edge — the case the edge sweep's
 * column-walk window handles.  Public API only.
 */

#include <stdio.h>
#include <hb.h>
#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *rdr = hb_raster_draw_create_or_fail ();
  if (!rdr)
    return 1;

  /* Fix the output extents to an 8x2 surface; the outline extends
   * past its right edge, like a negative-margin crop. */
  hb_raster_extents_t ext = {0, 0, 8, 2, 0};
  hb_raster_draw_set_extents (rdr, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (rdr);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  hb_draw_move_to (df, rdr, &st, 11.5f, 0.02f);
  hb_draw_line_to (df, rdr, &st, 4.0f, 1.02f);
  hb_draw_line_to (df, rdr, &st, 4.0f, 0.02f);
  hb_draw_close_path (df, rdr, &st);

  hb_raster_image_t *img = hb_raster_draw_render (rdr);
  if (img)
  {
    hb_raster_extents_t out;
    hb_raster_image_get_extents (img, &out);
    const uint8_t *buf = hb_raster_image_get_buffer (img);
    printf ("rendered %ux%u stride %u pixel(5,0)=%u\n",
	    out.width, out.height, out.stride,
	    out.height ? buf[0 * out.stride + 5] : 0);
    hb_raster_image_destroy (img);
  }

  hb_raster_draw_destroy (rdr);
  return 0;
}
