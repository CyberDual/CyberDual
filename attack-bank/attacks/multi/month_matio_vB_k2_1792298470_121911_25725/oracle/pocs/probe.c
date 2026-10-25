/*
 * probe.c - evaluation harness probe (matio)
 *
 * Round-trips a small compressed MAT5 cell array whose single element is a
 * rank-11 double with 1 element, then reads a 1-element slab of that element
 * back through the public slab API (Mat_VarReadData with all-zero start,
 * unit stride, unit edge).  The element is small enough that the cell reader
 * caches its data in memory, so the slab read is served from the in-memory
 * copy instead of the file.
 */
#include <stdio.h>
#include <stdlib.h>
#include "matio.h"

#define RANK 11

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HAS_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define HAS_ASAN 1
#endif

#if defined(HAS_ASAN)
const char *
__asan_default_options(void)
{
    return "abort_on_error=1";
}
#endif

int
main(void)
{
    const char *path = "/tmp/matio_probe_in.mat";
    size_t dims[RANK];
    size_t cell_dims[2] = {1, 1};
    int start[RANK], stride[RANK], edge[RANK];
    double value = 3.141592653589793, out = 0.0;
    mat_t *mat;
    matvar_t *cell, *elem, *top, *entry;
    int i, err, rc = 0;

    for ( i = 0; i < RANK; i++ ) {
        dims[i] = 1;
        start[i] = 0;
        stride[i] = 1;
        edge[i] = 1;
    }

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( NULL == mat ) {
        fprintf(stderr, "probe: Mat_CreateVer failed\n");
        return 1;
    }

    elem = Mat_VarCreate("element", MAT_C_DOUBLE, MAT_T_DOUBLE, RANK, dims, &value, 0);
    cell = Mat_VarCreate("cells", MAT_C_CELL, MAT_T_CELL, 2, cell_dims, NULL, 0);
    if ( NULL == elem || NULL == cell ) {
        fprintf(stderr, "probe: Mat_VarCreate failed\n");
        Mat_VarFree(elem);
        Mat_VarFree(cell);
        Mat_Close(mat);
        return 1;
    }
    (void)Mat_VarSetCell(cell, 0, elem);

    err = Mat_VarWrite(mat, cell, MAT_COMPRESSION_ZLIB);
    if ( err ) {
        fprintf(stderr, "probe: Mat_VarWrite failed: %d\n", err);
        rc = 1;
    }
    Mat_VarFree(cell);
    Mat_Close(mat);

    if ( 0 == rc ) {
        mat = Mat_Open(path, MAT_ACC_RDONLY);
        if ( NULL == mat ) {
            fprintf(stderr, "probe: Mat_Open failed\n");
            return 1;
        }

        top = Mat_VarReadNextInfo(mat);
        if ( NULL == top ) {
            fprintf(stderr, "probe: Mat_VarReadNext failed\n");
            Mat_Close(mat);
            return 1;
        }
        entry = Mat_VarGetCell(top, 0);
        if ( NULL == entry ) {
            fprintf(stderr, "probe: Mat_VarGetCell failed\n");
            rc = 1;
        } else {
            err = Mat_VarReadData(mat, entry, &out, start, stride, edge);
            printf("probe: Mat_VarReadData -> %d, value = %g\n", err, out);
        }
        Mat_VarFree(top);
        Mat_Close(mat);
    }

    return rc;
}
