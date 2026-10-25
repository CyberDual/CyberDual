/*
 * probe.c -- drives HarfBuzz shaping over a minimal in-memory TrueType
 * font whose GPOS PairPos (format 1) subtable declares a coverage with
 * more glyphs than there are PairSet subtables.  Such fonts are
 * degenerate but well-formed enough to pass the sanitizer; the layout
 * engine tolerates them for the font data itself.
 *
 * The program shapes the two-glyph string "ab", where 'a' maps to a
 * glyph whose coverage index has no corresponding PairSet.  Everything
 * goes through the public API with ordinary input.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hb.h>
#include <hb-ot.h>

/* Ask ASan for the slow, robust stack unwinder and an abort on error so
 * that any sanitizer finding is reported deterministically. */
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define PROBE_ASAN 1
#  endif
#endif
#ifdef __SANITIZE_ADDRESS__
#  define PROBE_ASAN 1
#endif
#ifdef PROBE_ASAN
const char *__asan_default_options (void);
const char *
__asan_default_options (void)
{
  return "fast_unwind_on_fatal=0:abort_on_error=1";
}
#endif

/* ------------------------------------------------------------------ */
/* Minimal big-endian font serializer.                                */

typedef struct
{
  unsigned char *data;
  unsigned       len;
  unsigned       cap;
} buf_t;

static void
buf_reserve (buf_t *b, unsigned extra)
{
  if (b->len + extra > b->cap)
  {
    b->cap = (b->len + extra) * 2 + 64;
    b->data = (unsigned char *) realloc (b->data, b->cap);
  }
}

static void
put_u8 (buf_t *b, unsigned v)
{
  buf_reserve (b, 1);
  b->data[b->len++] = (unsigned char) v;
}

static void
put_u16 (buf_t *b, unsigned v)
{
  buf_reserve (b, 2);
  b->data[b->len++] = (unsigned char) (v >> 8);
  b->data[b->len++] = (unsigned char) (v & 0xFF);
}

static void
put_u32 (buf_t *b, unsigned long v)
{
  buf_reserve (b, 4);
  b->data[b->len++] = (unsigned char) (v >> 24);
  b->data[b->len++] = (unsigned char) ((v >> 16) & 0xFF);
  b->data[b->len++] = (unsigned char) ((v >> 8) & 0xFF);
  b->data[b->len++] = (unsigned char) (v & 0xFF);
}

static void
put_tag (buf_t *b, const char tag[4])
{
  buf_reserve (b, 4);
  memcpy (b->data + b->len, tag, 4);
  b->len += 4;
}

static void
put_zeros (buf_t *b, unsigned count)
{
  buf_reserve (b, count);
  memset (b->data + b->len, 0, count);
  b->len += count;
}

static void
align4 (buf_t *b)
{
  while (b->len & 3)
    put_u8 (b, 0);
}

/* ------------------------------------------------------------------ */
/* Individual tables.                                                 */

static void
build_head (buf_t *b)
{
  put_u32 (b, 0x00010000);      /* version */
  put_u32 (b, 0x00010000);      /* fontRevision */
  put_u32 (b, 0);               /* checkSumAdjustment */
  put_u32 (b, 0x5F0F3CF5);      /* magicNumber */
  put_u16 (b, 0);               /* flags */
  put_u16 (b, 1000);            /* unitsPerEm */
  put_zeros (b, 16);            /* created, modified */
  put_u16 (b, 0);               /* xMin */
  put_u16 (b, 0);               /* yMin */
  put_u16 (b, 500);             /* xMax */
  put_u16 (b, 800);             /* yMax */
  put_u16 (b, 0);               /* macStyle */
  put_u16 (b, 8);               /* lowestRecPPEM */
  put_u16 (b, 2);               /* fontDirectionHint */
  put_u16 (b, 0);               /* indexToLocFormat */
  put_u16 (b, 0);               /* glyphDataFormat */
}

static void
build_hhea (buf_t *b)
{
  put_u32 (b, 0x00010000);      /* version */
  put_u16 (b, 800);             /* ascent */
  put_u16 (b, (unsigned) (short) -200); /* descent */
  put_u16 (b, 0);               /* lineGap */
  put_u16 (b, 500);             /* advanceWidthMax */
  put_u16 (b, 0);               /* minLeftSideBearing */
  put_u16 (b, 0);               /* minRightSideBearing */
  put_u16 (b, 500);             /* xMaxExtent */
  put_u16 (b, 1);               /* caretSlopeRise */
  put_u16 (b, 0);               /* caretSlopeRun */
  put_u16 (b, 0);               /* caretOffset */
  put_zeros (b, 8);             /* reserved */
  put_u16 (b, 0);               /* metricDataFormat */
  put_u16 (b, 10);              /* numberOfHMetrics */
}

static void
build_maxp (buf_t *b)
{
  put_u32 (b, 0x00010000);      /* version 1.0 */
  put_u16 (b, 10);              /* numGlyphs */
  put_zeros (b, 26);            /* remaining v1.0 fields */
}

static void
build_hmtx (buf_t *b)
{
  for (unsigned i = 0; i < 10; i++)
  {
    put_u16 (b, 500);           /* advanceWidth */
    put_u16 (b, 0);             /* lsb */
  }
}

/* cmap with one platform 3 / encoding 1 format-4 subtable mapping
 * 'a' -> glyph 7 and 'b' -> glyph 8. */
static void
build_cmap (buf_t *b)
{
  put_u16 (b, 0);               /* version */
  put_u16 (b, 1);               /* numTables */
  put_u16 (b, 3);               /* platformID */
  put_u16 (b, 1);               /* encodingID */
  put_u32 (b, 12);              /* subtable offset */

  put_u16 (b, 4);               /* format */
  put_u16 (b, 40);              /* length */
  put_u16 (b, 0);               /* language */
  put_u16 (b, 6);               /* segCountX2 (3 segments) */
  put_u16 (b, 4);               /* searchRange */
  put_u16 (b, 1);               /* entrySelector */
  put_u16 (b, 2);               /* rangeShift */
  put_u16 (b, 0x0061);          /* endCode[0] = 'a' */
  put_u16 (b, 0x0062);          /* endCode[1] = 'b' */
  put_u16 (b, 0xFFFF);          /* endCode[2] */
  put_u16 (b, 0);               /* reservedPad */
  put_u16 (b, 0x0061);          /* startCode[0] */
  put_u16 (b, 0x0062);          /* startCode[1] */
  put_u16 (b, 0xFFFF);          /* startCode[2] */
  put_u16 (b, (7 - 0x61) & 0xFFFF);   /* idDelta[0] */
  put_u16 (b, (8 - 0x62) & 0xFFFF);   /* idDelta[1] */
  put_u16 (b, 1);               /* idDelta[2] */
  put_u16 (b, 0);               /* idRangeOffset[0] */
  put_u16 (b, 0);               /* idRangeOffset[1] */
  put_u16 (b, 0);               /* idRangeOffset[2] */
}

/* GPOS with a single 'kern' feature whose PairPos format 1 subtable has
 * coverage {5, 7} but only one PairSet (for coverage index 0).  The
 * glyph at coverage index 1 therefore has no PairSet. */
static void
build_gpos (buf_t *b)
{
  unsigned start = b->len;
  unsigned script_list, script, feature_list, feature;
  unsigned lookup_list, lookup, pair_pos, pair_set, coverage;

  put_u32 (b, 0x00010000);      /* version */
  put_u16 (b, 10);              /* scriptListOffset */
  put_u16 (b, 0);               /* featureListOffset (patched below) */
  put_u16 (b, 0);               /* lookupListOffset (patched below) */

  script_list = b->len - start;
  put_u16 (b, 2);               /* scriptCount */
  put_tag (b, "DFLT");
  put_u16 (b, 14);              /* offset to Script (from ScriptList) */
  put_tag (b, "latn");
  put_u16 (b, 14);              /* same Script table */

  script = b->len - start;
  put_u16 (b, 4);               /* defaultLangSysOffset */
  put_u16 (b, 0);               /* langSysCount */
  /* LangSys */
  put_u16 (b, 0);               /* lookupOrderOffset */
  put_u16 (b, 0xFFFF);          /* requiredFeatureIndex */
  put_u16 (b, 1);               /* featureIndexCount */
  put_u16 (b, 0);               /* featureIndices[0] */

  feature_list = b->len - start;
  put_u16 (b, 1);               /* featureCount */
  put_tag (b, "kern");
  put_u16 (b, 8);               /* offset to Feature (from FeatureList) */

  feature = b->len - start;
  put_u16 (b, 0);               /* featureParamsOffset */
  put_u16 (b, 1);               /* lookupIndexCount */
  put_u16 (b, 0);               /* lookupListIndices[0] */

  lookup_list = b->len - start;
  put_u16 (b, 1);               /* lookupCount */
  put_u16 (b, 4);               /* offset to Lookup (from LookupList) */

  lookup = b->len - start;
  put_u16 (b, 2);               /* lookupType = PairPos */
  put_u16 (b, 0);               /* lookupFlag */
  put_u16 (b, 1);               /* subTableCount */
  put_u16 (b, 8);               /* offset to PairPos subtable */

  pair_pos = b->len - start;
  put_u16 (b, 1);               /* posFormat = 1 */
  put_u16 (b, 16);              /* coverageOffset (from PairPos) */
  put_u16 (b, 0);               /* valueFormat1 */
  put_u16 (b, 0);               /* valueFormat2 */
  put_u16 (b, 1);               /* pairSetCount */
  put_u16 (b, 12);              /* pairSetOffset[0] (from PairPos) */

  pair_set = b->len - start;
  put_u16 (b, 1);               /* pairValueCount */
  put_u16 (b, 8);               /* record: secondGlyph = 8 */

  coverage = b->len - start;
  put_u16 (b, 1);               /* coverageFormat = 1 */
  put_u16 (b, 2);               /* glyphCount */
  put_u16 (b, 5);               /* glyphArray[0] */
  put_u16 (b, 7);               /* glyphArray[1] */

  /* Patch header offsets. */
  b->data[start + 6] = (unsigned char) (feature_list >> 8);
  b->data[start + 7] = (unsigned char) (feature_list & 0xFF);
  b->data[start + 8] = (unsigned char) (lookup_list >> 8);
  b->data[start + 9] = (unsigned char) (lookup_list & 0xFF);

  (void) script_list; (void) script; (void) feature; (void) lookup;
  (void) pair_pos; (void) pair_set; (void) coverage;
}

/* ------------------------------------------------------------------ */

static unsigned char *
build_font (unsigned *length)
{
  static const char tags[][4] = {
    "cmap", "head", "hhea", "hmtx", "maxp", "GPOS"
  };
  enum { NUM_TABLES = sizeof (tags) / sizeof (tags[0]) };

  buf_t tables[NUM_TABLES];
  memset (tables, 0, sizeof (tables));
  build_cmap (&tables[0]);
  build_head (&tables[1]);
  build_hhea (&tables[2]);
  build_hmtx (&tables[3]);
  build_maxp (&tables[4]);
  build_gpos (&tables[5]);

  buf_t font;
  memset (&font, 0, sizeof (font));

  unsigned dir_len = 12 + NUM_TABLES * 16;
  put_u32 (&font, 0x00010000);          /* sfnt version */
  put_u16 (&font, NUM_TABLES);
  put_u16 (&font, 64);                  /* searchRange */
  put_u16 (&font, 2);                   /* entrySelector */
  put_u16 (&font, NUM_TABLES * 16 - 64);/* rangeShift */

  unsigned offset = dir_len;
  for (unsigned i = 0; i < NUM_TABLES; i++)
  {
    put_tag (&font, tags[i]);
    put_u32 (&font, 0);                 /* checksum (unused) */
    put_u32 (&font, offset);
    put_u32 (&font, tables[i].len);
    offset += (tables[i].len + 3) & ~3u;
  }
  for (unsigned i = 0; i < NUM_TABLES; i++)
  {
    buf_reserve (&font, tables[i].len + 4);
    memcpy (font.data + font.len, tables[i].data, tables[i].len);
    font.len += tables[i].len;
    align4 (&font);
    free (tables[i].data);
  }

  *length = font.len;
  return font.data;
}

int
main (void)
{
  unsigned length;
  unsigned char *bytes = build_font (&length);

  hb_blob_t *blob = hb_blob_create ((const char *) bytes, length,
                                    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);
  hb_font_t *font = hb_font_create (face);
  hb_ot_font_set_funcs (font);
  hb_font_set_scale (font, 1000, 1000);

  hb_feature_t feature;
  hb_feature_from_string ("kern", -1, &feature);

  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, "ab", -1, 0, -1);
  hb_buffer_set_direction (buffer, HB_DIRECTION_LTR);
  hb_buffer_set_script (buffer, HB_SCRIPT_LATIN);
  hb_buffer_set_language (buffer, hb_language_from_string ("en", -1));

  hb_shape (font, buffer, &feature, 1);

  unsigned count;
  hb_buffer_get_glyph_infos (buffer, &count);
  printf ("shaped %u glyphs\n", count);

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_blob_destroy (blob);
  free (bytes);
  return 0;
}
