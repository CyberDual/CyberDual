/*
 * Drive the subset depend API over a minimal font that carries a GSUB
 * contextual substitution lookup (ContextSubst format 1, one rule set).
 * The font is embedded so the driver is self-contained.  This mirrors
 * what the depend fuzzers do: build a face from a caller-owned buffer,
 * compile the dependency graph, then walk every glyph's dependency
 * entries and the sets they reference.
 *
 * Build against an AddressSanitizer build of harfbuzz (the depend API
 * requires -Dexperimental_api=true):
 *
 *   cc -fsanitize=address -g -I<srcdir>/src -I<builddir>/src newbug.c \
 *      -o newbug -L<builddir>/src -lharfbuzz -lharfbuzz-subset \
 *      -Wl,-rpath,<builddir>/src
 *
 * Run:
 *
 *   ASAN_OPTIONS=abort_on_error=1 ./newbug
 */

#include <stdlib.h>
#include <string.h>

#include "hb.h"
#include "hb-subset.h"

/* sfnt: maxp (numGlyphs 16) + GSUB (feature "test" -> Lookup 0 type 5
 * format 1, Coverage {5}, RuleSet[0] with a single rule). */
static const unsigned char font_data[] = {
  0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00,
  0x47, 0x53, 0x55, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32,
  0x00, 0x00, 0x00, 0x50, 0x6d, 0x61, 0x78, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x50, 0x00,
  0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x1e, 0x00, 0x2c,
  0x00, 0x01, 0x44, 0x46, 0x4c, 0x54, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x00, 0xff, 0xff, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x74, 0x65,
  0x73, 0x74, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x04, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01,
  0x00, 0x08, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00, 0x05,
  0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05,
};

int main (void)
{
  unsigned int size = sizeof (font_data);

  /* Own copy at exact size so the buffer ends with the last GSUB field. */
  unsigned char *data = (unsigned char *) malloc (size);
  if (!data) return 1;
  memcpy (data, font_data, size);

  hb_blob_t *blob = hb_blob_create ((const char *) data, size,
                                    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);

  hb_subset_depend_t *depend = hb_subset_depend_from_face_or_fail (face);
  if (depend)
  {
    unsigned int num_glyphs = hb_face_get_glyph_count (face);
    hb_set_t *temp_set = hb_set_create ();

    for (unsigned int gid = 0; gid < num_glyphs; gid++)
    {
      unsigned int total =
        hb_subset_depend_lookup_glyph (depend, gid, 0, NULL, NULL);

      for (unsigned int index = 0; index < total; index++)
      {
        hb_subset_depend_entry_t entry;
        unsigned int count = 1;
        hb_subset_depend_lookup_glyph (depend, gid, index, &count, &entry);

        if (entry.ligature_set_index != HB_CODEPOINT_INVALID)
          hb_subset_depend_lookup_set (depend, entry.ligature_set_index,
                                       temp_set);

        if (entry.context_set_index != HB_CODEPOINT_INVALID)
          hb_subset_depend_lookup_set (depend, entry.context_set_index,
                                       temp_set);
      }
    }

    hb_set_destroy (temp_set);
    hb_subset_depend_destroy (depend);
  }

  hb_face_destroy (face);
  hb_blob_destroy (blob);
  free (data);
  return 0;
}
