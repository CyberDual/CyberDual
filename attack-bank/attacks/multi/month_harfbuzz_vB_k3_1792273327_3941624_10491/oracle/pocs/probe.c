#include <hb-gpu.h>

#include <stddef.h>

static const hb_color_stop_t stops[] = {
  { 0.0f, 0.25f, HB_COLOR (0, 0, 255, 255) },
  { 0.5f, 0.75f, HB_COLOR (255, 0, 0, 255) },
  { 1.0f, 1.0f, HB_COLOR (0, 255, 0, 255) },
};

static unsigned int
get_color_stops (hb_color_line_t *color_line,
		 void *color_line_data,
		 unsigned int start,
		 unsigned int *count,
		 hb_color_stop_t *color_stops,
		 void *user_data)
{
  (void) color_line;
  (void) color_line_data;
  (void) user_data;
  if (start >= 3)
  {
    if (count)
      *count = 0;
    return 3;
  }

  if (count)
  {
    unsigned int remaining = 3 - start;
    unsigned int requested = *count;
    if (requested > remaining)
      requested = remaining;
    for (unsigned int i = 0; i < requested; i++)
      color_stops[i] = stops[start + i];
    *count = requested;
  }

  return 3;
}

static hb_paint_extend_t
get_extend (hb_color_line_t *color_line,
	    void *color_line_data,
	    void *user_data)
{
  (void) color_line;
  (void) color_line_data;
  (void) user_data;
  return HB_PAINT_EXTEND_PAD;
}

int
main (void)
{
  hb_gpu_paint_t *paint = hb_gpu_paint_create_or_fail ();
  if (!paint)
    return 1;

  hb_gpu_paint_set_scale (paint, 1000, 1000);
  hb_paint_funcs_t *funcs = hb_gpu_paint_get_funcs (paint);

  void *draw_data = NULL;
  hb_draw_funcs_t *draw_funcs = hb_paint_push_clip_path_start (funcs, paint,
							       &draw_data);
  if (draw_funcs)
  {
    hb_draw_state_t state = HB_DRAW_STATE_DEFAULT;
    hb_draw_move_to (draw_funcs, draw_data, &state, 0.f, 0.f);
    hb_draw_line_to (draw_funcs, draw_data, &state, 100.f, 0.f);
    hb_draw_line_to (draw_funcs, draw_data, &state, 100.f, 100.f);
    hb_draw_close_path (draw_funcs, draw_data, &state);
  }
  hb_paint_push_clip_path_end (funcs, paint);

  hb_color_line_t color_line = {0};
  color_line.get_color_stops = get_color_stops;
  color_line.get_extend = get_extend;
  hb_paint_linear_gradient (funcs, paint, &color_line,
			    0.f, 0.f, 100.f, 0.f,
			    0.f, 100.f);
  hb_paint_pop_clip (funcs, paint);

  hb_blob_t *blob = hb_gpu_paint_encode (paint, NULL);
  hb_blob_destroy (blob);
  hb_gpu_paint_destroy (paint);
  return 0;
}
