#include <hb.h>
#include <hb-subset.h>

#include <stdlib.h>
#include <string.h>

static void
put16 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char) (v >> 8);
  p[1] = (unsigned char) v;
}

static void
put32 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char) (v >> 24);
  p[1] = (unsigned char) (v >> 16);
  p[2] = (unsigned char) (v >> 8);
  p[3] = (unsigned char) v;
}

/* Minimal sfnt: maxp plus a GSUB ContextSubst lookup whose rule is at EOF. */
static unsigned char *
build_font (unsigned *length)
{
  const unsigned input_count = 64;
  const unsigned input_stored = input_count - 1;
  unsigned char gsub[512];
  unsigned used = 0;
  unsigned script_list, script, langsys, feature_list, feature;
  unsigned lookup_list, lookup, ctx, cov, ruleset, rule, i;

  memset (gsub, 0, sizeof gsub);
  put32 (gsub, 0x00010000u);
  used = 10;

  script_list = used;
  put16 (gsub + used, 1);
  memcpy (gsub + used + 2, "DFLT", 4);
  used += 8;

  script = used;
  used += 4;

  langsys = used;
  put16 (gsub + used, 0);
  put16 (gsub + used + 2, 0xFFFFu);
  put16 (gsub + used + 4, 1);
  put16 (gsub + used + 6, 0);
  used += 8;

  put16 (gsub + script, langsys - script);
  put16 (gsub + script_list + 6, script - script_list);

  feature_list = used;
  put16 (gsub + used, 1);
  memcpy (gsub + used + 2, "calt", 4);
  used += 8;

  feature = used;
  put16 (gsub + used, 0);
  put16 (gsub + used + 2, 1);
  put16 (gsub + used + 4, 0);
  used += 6;
  put16 (gsub + feature_list + 6, feature - feature_list);

  lookup_list = used;
  put16 (gsub + used, 1);
  used += 4;

  lookup = used;
  put16 (gsub + used, 5);
  put16 (gsub + used + 2, 0);
  put16 (gsub + used + 4, 1);
  used += 8;
  put16 (gsub + lookup_list + 2, lookup - lookup_list);

  ctx = used;
  put16 (gsub + used, 1);
  put16 (gsub + used + 4, 1);
  used += 8;
  put16 (gsub + lookup + 6, ctx - lookup);

  cov = used;
  put16 (gsub + used, 1);
  put16 (gsub + used + 2, 1);
  put16 (gsub + used + 4, 1);
  used += 6;
  put16 (gsub + ctx + 2, cov - ctx);

  ruleset = used;
  put16 (gsub + used, 1);
  used += 4;
  put16 (gsub + ctx + 6, ruleset - ctx);

  rule = used;
  put16 (gsub + used, input_count);
  put16 (gsub + used + 2, 0);
  used += 4;
  for (i = 0; i < input_stored; i++)
    put16 (gsub + used + i * 2, 1);
  used += input_stored * 2;
  put16 (gsub + ruleset + 2, rule - ruleset);

  put16 (gsub + 4, script_list);
  put16 (gsub + 6, feature_list);
  put16 (gsub + 8, lookup_list);

  {
    const unsigned maxp_off = 44;
    const unsigned gsub_off = 52;
    unsigned char *font;
    unsigned len = gsub_off + used;

    font = (unsigned char *) malloc (len);
    if (!font)
      return NULL;
    memset (font, 0, len);
    put32 (font, 0x00010000u);
    put16 (font + 4, 2);
    put16 (font + 6, 32);
    put16 (font + 8, 1);
    put16 (font + 10, 0);
    memcpy (font + 12, "GSUB", 4);
    put32 (font + 16, 0);
    put32 (font + 20, gsub_off);
    put32 (font + 24, used);
    memcpy (font + 28, "maxp", 4);
    put32 (font + 32, 0);
    put32 (font + 36, maxp_off);
    put32 (font + 40, 6);
    put32 (font + maxp_off, 0x00005000u);
    put16 (font + maxp_off + 4, 4);
    memcpy (font + gsub_off, gsub, used);
    *length = len;
    return font;
  }
}

int
main (void)
{
  unsigned length = 0;
  unsigned char *data = build_font (&length);
  hb_blob_t *blob;
  hb_face_t *face;
  hb_subset_depend_t *depend;

  if (!data)
    return 1;

  blob = hb_blob_create ((const char *) data, length,
			 HB_MEMORY_MODE_READONLY, data, free);
  face = hb_face_create (blob, 0);
  depend = hb_subset_depend_from_face_or_fail (face);
  if (depend)
    hb_subset_depend_destroy (depend);
  hb_face_destroy (face);
  hb_blob_destroy (blob);
  return 0;
}
