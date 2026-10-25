/*
 * Probe: rank-2 slab read of an eagerly cached (in-memory) MAT5 variable.
 *
 * Drives only the public matio API:
 *   Mat_CreateVer / Mat_VarCreate / Mat_VarCreateStruct / Mat_VarSetStructFieldByName
 *   Mat_VarWrite / Mat_Close / Mat_Open / Mat_VarReadInfo
 *   Mat_VarGetStructFieldByName / Mat_VarReadData
 *
 * A 4x4 double field is stored (compressed) inside a 1x1 struct; reading the
 * struct's header caches the small field data in memory, so the slab read runs
 * through the in-memory rank-2 copy.  The requested selection starts one column
 * past the last column of the array (start = {0, 4} for dims = {4, 4}); the
 * library is expected to reject it with MATIO_E_BAD_ARGUMENT.
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
    const char *path = "/tmp/matio_probe_slab.mat";
    const char *fieldnames[2] = {"x", NULL};
    size_t sdims[2] = {1, 1};
    size_t fdims[2] = {4, 4};
    double fdata[16];
    int start[2] = {0, 4};
    int stride[2] = {1, 1};
    int edge[2] = {4, 2};
    double buf[8];
    mat_t *mat, *wmat;
    matvar_t *st, *field, *slab_src;
    int i, err;

    for ( i = 0; i < 16; i++ )
        fdata[i] = (double)i;

    /* Write a compressed v5 file holding s.x = 4x4 double */
    wmat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( wmat == NULL )
        return 1;
    slab_src = Mat_VarCreate("x", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, fdims, fdata, 0);
    st = Mat_VarCreateStruct2("s", 2, sdims, fieldnames);
    if ( slab_src == NULL || st == NULL ) {
        Mat_Close(wmat);
        return 1;
    }
    Mat_VarSetStructFieldByName(st, "x", 0, slab_src);
    if ( Mat_VarWrite(wmat, st, MAT_COMPRESSION_ZLIB) )
        return 1;
    Mat_VarFree(st);
    Mat_Close(wmat);

    /* Read the struct's header and take the slab of the cached field data */
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;
    st = Mat_VarReadInfo(mat, "s");
    if ( st == NULL ) {
        Mat_Close(mat);
        return 1;
    }
    field = Mat_VarGetStructFieldByName(st, "x", 0);
    if ( field == NULL || field->rank != 2 || field->dims[0] != 4 || field->dims[1] != 4 ) {
        Mat_VarFree(st);
        Mat_Close(mat);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    err = Mat_VarReadData(mat, field, buf, start, stride, edge);
    printf("Mat_VarReadData(%s) = %d\n", path, err);

    Mat_VarFree(st);
    Mat_Close(mat);
    remove(path);
    return 0;
}
