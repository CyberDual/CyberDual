#include "hb.h"
#include "hb-subset.h"

int main (void)
{
  static const unsigned char cff[] = {
    1, 0, 4, 1,
    0, 1, 1, 1, 2, 'A',
    0, 1, 1, 1, 32,
    139, 139, 139, 12, 30,
    29, 0, 0, 0, 50, 15,
    29, 0, 0, 0, 51, 17,
    29, 0, 0, 0, 57, 12, 36,
    29, 0, 0, 0, 62, 12, 37,
    0, 0,
    0, 0,
    0,
    0, 1, 1, 1, 2, 14,
    0, 1, 1, 1, 1,
    0, 0
  };
  static const unsigned char maxp[] = {0, 0, 0x50, 0, 0, 1};
  hb_face_t *builder = hb_face_builder_create ();
  hb_blob_t *table = hb_blob_create ((const char *) cff, sizeof (cff), HB_MEMORY_MODE_READONLY, 0, 0);
  hb_face_builder_add_table (builder, HB_TAG ('C', 'F', 'F', ' '), table);
  hb_blob_destroy (table);
  table = hb_blob_create ((const char *) maxp, sizeof (maxp), HB_MEMORY_MODE_READONLY, 0, 0);
  hb_face_builder_add_table (builder, HB_TAG ('m', 'a', 'x', 'p'), table);
  hb_blob_destroy (table);
  hb_blob_t *sfnt = hb_face_reference_blob (builder);
  hb_face_t *face = hb_face_create (sfnt, 0);
  hb_blob_destroy (sfnt);
  hb_face_destroy (builder);

  hb_subset_input_t *input = hb_subset_input_create_or_fail ();
  hb_set_add (hb_subset_input_glyph_set (input), 0);
  hb_face_t *out = hb_subset_or_fail (face, input);
  if (!out)
    return 2;
  hb_face_destroy (out);
  hb_subset_input_destroy (input);
  hb_face_destroy (face);
  return 0;
}
