#include <libheif/heif.h>
#include <stdio.h>

static int check(heif_error error, const char* operation)
{
  if (error.code == heif_error_Ok) {
    return 0;
  }
  fprintf(stderr, "%s: %s\n", operation, error.message);
  return 1;
}

int main(void)
{
  heif_image* image = NULL;
  uint32_t component_id;

  if (check(heif_image_create(128, 128, heif_colorspace_custom,
                              heif_chroma_planar, &image), "create")) {
    return 1;
  }
  if (check(heif_image_add_component(image, 128, 128,
                                     heif_cmpd_component_type_filter_array,
                                     heif_component_datatype_unsigned_integer,
                                     8, &component_id), "filter array")) {
    return 1;
  }
  if (check(heif_image_add_component(image, 128, 1,
                                     heif_cmpd_component_type_disparity,
                                     heif_component_datatype_unsigned_integer,
                                     8, &component_id), "disparity")) {
    return 1;
  }

  heif_error result = heif_image_crop(image, 0, 0, 0, 0);
  if (result.code == heif_error_Unsupported_feature) {
    heif_image_release(image);
    return 0;
  }
  if (check(result, "crop")) {
    heif_image_release(image);
    return 1;
  }
  heif_image_release(image);
  return 0;
}
