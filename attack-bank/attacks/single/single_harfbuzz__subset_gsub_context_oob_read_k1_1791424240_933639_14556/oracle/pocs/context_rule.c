#include <hb.h>
#include <hb-ot.h>
#include <hb-subset.h>

#include <stdint.h>

static const uint8_t gsub[] = {
  0, 1, 0, 0, 0, 10, 0, 30, 0, 44,
  0, 1, 'D', 'F', 'L', 'T', 0, 8,
  0, 4, 0, 0,
  0, 0, 0xff, 0xff, 0, 1, 0, 0,
  0, 1, 'l', 'i', 'g', 'a', 0, 8,
  0, 0, 0, 1, 0, 0,
  0, 1, 0, 4,
  0, 5, 0, 0, 0, 1, 0, 8,
  0, 1, 0, 8, 0, 1, 0, 14,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 4,
  0, 1, 0, 0
};

static const uint8_t maxp[] = {0, 0, 0x50, 0, 0, 2};

int main (void)
{
  hb_face_t *face = hb_face_builder_create ();
  hb_blob_t *gsub_blob = hb_blob_create ((const char *) gsub, sizeof gsub,
                                       HB_MEMORY_MODE_DUPLICATE, NULL, NULL);
  hb_blob_t *maxp_blob = hb_blob_create ((const char *) maxp, sizeof maxp,
                                       HB_MEMORY_MODE_DUPLICATE, NULL, NULL);
  hb_face_builder_add_table (face, HB_TAG ('G', 'S', 'U', 'B'), gsub_blob);
  hb_face_builder_add_table (face, HB_TAG ('m', 'a', 'x', 'p'), maxp_blob);
  hb_blob_destroy (gsub_blob);
  hb_blob_destroy (maxp_blob);
  if (hb_face_get_glyph_count (face) != 2 ||
      hb_ot_layout_table_get_lookup_count (face, HB_OT_TAG_GSUB) != 1 ||
      !hb_ot_layout_has_substitution (face))
    return 2;
  hb_subset_depend_t *depend = hb_subset_depend_from_face_or_fail (face);
  hb_subset_depend_destroy (depend);
  hb_face_destroy (face);
  return 0;
}
