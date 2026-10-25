/*
 * Probe: off-surface steep edge in the analytic rasterizer sweep.
 *
 * Drives the public raster API: a single closed rectangle whose right
 * edge sits a million pixels right of the left edge.  The auto-sized
 * surface clamps its width to HB_RASTER_MAX_AUTO_DIMENSION, so the
 * right (vertical, single-column) edge is far outside the surface.
 * The sweep must fold or skip off-surface columns; if it writes them
 * straight into the scanline accumulators, this aborts under
 * AddressSanitizer with a heap-buffer-overflow write.
 *
 * Build: cc probe.c -I<src> -lhsan... see selection.json.
 * Clean run: exits 0 and prints the rendered extent.
 */

#include <stdio.h>
#include "hb.h"
#include "hb-raster.h"

int
main (void)
{
  hb_raster_draw_t *rdr = hb_raster_draw_create_or_fail ();
  if (!rdr) return 0;

  /* Rectangle 1,000,000 px wide, 64 px tall.  The two vertical edges
   * are steep (single-column) edges; the right one lies ~996k columns
   * right of the 4096-column auto-sized surface. */
  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (rdr);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  hb_draw_move_to     (df, rdr, &st, 0.f,       0.f);
  hb_draw_line_to     (df, rdr, &st, 1000000.f, 0.f);
  hb_draw_line_to     (df, rdr, &st, 1000000.f, 64.f);
  hb_draw_line_to     (df, rdr, &st, 0.f,       64.f);
  hb_draw_close_path  (df, rdr, &st);

  hb_raster_image_t *img = hb_raster_draw_render (rdr);
  if (img)
  {
    hb_raster_extents_t ext;
    hb_raster_image_get_extents (img, &ext);
    printf ("rendered %ux%u at (%d,%d)\n",
	    ext.width, ext.height, ext.x_origin, ext.y_origin);
    hb_raster_image_destroy (img);
  }

  hb_raster_draw_destroy (rdr);
  return 0;
}
