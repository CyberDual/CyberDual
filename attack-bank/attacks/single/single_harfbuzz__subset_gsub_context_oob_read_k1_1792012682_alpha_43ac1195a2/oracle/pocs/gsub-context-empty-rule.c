/*
 * Exercises hb_subset_depend_from_face_or_fail() with a GSUB
 * ContextSubst Format 1 lookup whose single rule declares
 * inputCount = 0.
 */

#include <hb.h>
#include <hb-subset.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_GLYPHS 4
#define RULE_INPUT_COUNT 0

static void put16 (unsigned char *p, unsigned v) { p[0] = v >> 8; p[1] = v; }
static void put32 (unsigned char *p, unsigned v)
{
  p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}
static void put_tag (unsigned char *p, const char *t)
{
  p[0] = t[0]; p[1] = t[1]; p[2] = t[2]; p[3] = t[3];
}

int main (void)
{
  const unsigned rule_input_len =
    RULE_INPUT_COUNT ? RULE_INPUT_COUNT - 1 : 0;
  const unsigned rule_size = 4 + 2 * rule_input_len;
  const unsigned gsub_off = 52;
  const unsigned gsub_size = 74 + rule_size;
  const unsigned font_size = gsub_off + gsub_size;

  unsigned char *font = (unsigned char *) malloc (font_size);
  if (!font) return 1;
  memset (font, 0, font_size);

  /* sfnt header + table directory (GSUB, maxp) */
  put32 (font + 0, 0x00010000u);
  put16 (font + 4, 2);
  put16 (font + 6, 32);
  put16 (font + 8, 1);
  put16 (font + 10, 0);
  put_tag (font + 12, "GSUB");
  put32 (font + 20, gsub_off);
  put32 (font + 24, gsub_size);
  put_tag (font + 28, "maxp");
  put32 (font + 36, 44);
  put32 (font + 40, 6);

  /* maxp 0.5 */
  put32 (font + 44, 0x00005000u);
  put16 (font + 48, NUM_GLYPHS);

  unsigned char *g = font + gsub_off;

  /* GSUB header */
  put32 (g + 0, 0x00010000u);
  put16 (g + 4, 10);
  put16 (g + 6, 30);
  put16 (g + 8, 44);

  /* ScriptList: DFLT / default LangSys -> feature 0 */
  put16 (g + 10, 1);
  put_tag (g + 12, "DFLT");
  put16 (g + 16, 8);
  put16 (g + 18, 4);
  put16 (g + 20, 0);
  put16 (g + 22, 0);
  put16 (g + 24, 0xFFFF);
  put16 (g + 26, 1);
  put16 (g + 28, 0);

  /* FeatureList: liga -> lookup 0 */
  put16 (g + 30, 1);
  put_tag (g + 32, "liga");
  put16 (g + 36, 8);
  put16 (g + 38, 0);
  put16 (g + 40, 1);
  put16 (g + 42, 0);

  /* LookupList: ContextSubst format 1 */
  put16 (g + 44, 1);
  put16 (g + 46, 4);
  put16 (g + 48, 5);
  put16 (g + 50, 0);
  put16 (g + 52, 1);
  put16 (g + 54, 8);
  put16 (g + 56, 1);
  put16 (g + 58, 8);
  put16 (g + 60, 1);
  put16 (g + 62, 14);

  /* Coverage: glyph 1 */
  put16 (g + 64, 1);
  put16 (g + 66, 1);
  put16 (g + 68, 1);

  /* RuleSet with one rule that ends the font */
  put16 (g + 70, 1);
  put16 (g + 72, 4);
  put16 (g + 74, RULE_INPUT_COUNT);
  put16 (g + 76, 0);
  for (unsigned i = 0; i < rule_input_len; i++)
    put16 (g + 78 + 2 * i, 2);

  hb_blob_t *blob = hb_blob_create ((const char *) font, font_size,
				    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);

  hb_subset_depend_t *depend = hb_subset_depend_from_face_or_fail (face);
  fprintf (stderr, "%s\n", depend ? "depend graph built" : "depend graph failed");

  if (depend) hb_subset_depend_destroy (depend);
  hb_face_destroy (face);
  free (font);
  return 0;
}
