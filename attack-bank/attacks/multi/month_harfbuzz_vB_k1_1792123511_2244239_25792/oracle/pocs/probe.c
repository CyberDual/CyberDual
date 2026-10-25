/* Builds a minimal font with a VARC table whose coverage maps more
 * glyphs than there are records in the glyphRecords index, then asks
 * HarfBuzz to draw one of the covered glyphs via the public API. */

#include <hb.h>

#include <stdint.h>
#include <string.h>

static void put16 (uint8_t *p, uint16_t v) { p[0] = (uint8_t) (v >> 8); p[1] = (uint8_t) v; }
static void put32 (uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t) (v >> 24); p[1] = (uint8_t) (v >> 16);
  p[2] = (uint8_t) (v >> 8);  p[3] = (uint8_t) v;
}

#define FONT_LEN 318
#define VARC_OFF 60
#define VARC_LEN 194
#define HEAD_OFF 256
#define HEAD_LEN 54
#define MAXP_OFF 312
#define MAXP_LEN 6

static uint8_t font_data[FONT_LEN];

static void build_font (void)
{
  memset (font_data, 0, sizeof font_data);

  /* sfnt header */
  put32 (font_data + 0, 0x00010000u);
  put16 (font_data + 4, 3);   /* numTables */
  put16 (font_data + 6, 32);  /* searchRange */
  put16 (font_data + 8, 1);   /* entrySelector */
  put16 (font_data + 10, 16); /* rangeShift */

  /* table directory, tags sorted ascending: VARC, head, maxp */
  memcpy (font_data + 12, "VARC", 4);
  put32 (font_data + 20, VARC_OFF);
  put32 (font_data + 24, VARC_LEN);

  memcpy (font_data + 28, "head", 4);
  put32 (font_data + 36, HEAD_OFF);
  put32 (font_data + 40, HEAD_LEN);

  memcpy (font_data + 44, "maxp", 4);
  put32 (font_data + 52, MAXP_OFF);
  put32 (font_data + 56, MAXP_LEN);

  /* VARC table */
  uint8_t *v = font_data + VARC_OFF;
  put32 (v + 0, 0x00010000u); /* version 1.0 */
  put32 (v + 4, 24);          /* coverage */
  put32 (v + 8, 0);           /* varStore: none */
  put32 (v + 12, 0);          /* conditionList: none */
  put32 (v + 16, 0);          /* axisIndicesList: none */
  put32 (v + 20, 156);        /* glyphRecords */

  /* Coverage format 1: 64 glyphs, gids 10..73 */
  uint8_t *cov = v + 24;
  put16 (cov + 0, 1);
  put16 (cov + 2, 64);
  for (unsigned i = 0; i < 64; i++)
    put16 (cov + 4 + 2 * i, (uint16_t) (10 + i));

  /* glyphRecords: CFF2Index holding 8 three-byte records */
  uint8_t *idx = v + 156;
  put32 (idx + 0, 8);  /* count */
  idx[4] = 1;          /* offSize */
  for (unsigned i = 0; i <= 8; i++)
    idx[5 + i] = (uint8_t) (1 + 3 * i); /* 1-based offsets */
  for (unsigned i = 0; i < 8; i++)
  {
    idx[14 + 3 * i + 0] = 0x00;                  /* flags: none */
    idx[14 + 3 * i + 1] = 0x00;                  /* component gid hi */
    idx[14 + 3 * i + 2] = (uint8_t) (0x64 + i);  /* component gid lo: 100+i */
  }

  /* head */
  uint8_t *h = font_data + HEAD_OFF;
  put32 (h + 0, 0x00010000u);  /* version */
  put32 (h + 4, 0x00010000u);  /* fontRevision */
  put32 (h + 12, 0x5F0F3CF5u); /* magicNumber */
  put16 (h + 18, 1000);        /* unitsPerEm */
  put16 (h + 50, 0);           /* indexToLocFormat: short */

  /* maxp */
  uint8_t *m = font_data + MAXP_OFF;
  put32 (m + 0, 0x00005000u); /* version 0.5 */
  put16 (m + 4, 128);         /* numGlyphs */
}

int main (void)
{
  build_font ();

  hb_blob_t *blob = hb_blob_create ((const char *) font_data, FONT_LEN,
				    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_font_t *font = hb_font_create (face);
  hb_font_set_scale (font, 1000, 1000);

  hb_draw_funcs_t *funcs = hb_draw_funcs_create ();

  /* Glyph 18 is covered by the VARC coverage at index 8, one past the
   * last of the 8 glyph records. */
  hb_font_draw_glyph (font, 18, funcs, NULL);

  hb_glyph_extents_t extents;
  hb_font_get_glyph_extents (font, 18, &extents);

  hb_draw_funcs_destroy (funcs);
  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_blob_destroy (blob);

  return 0;
}
