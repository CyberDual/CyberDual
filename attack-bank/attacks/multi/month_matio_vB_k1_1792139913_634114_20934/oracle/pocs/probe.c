#include <stdlib.h>
#include "matio.h"

int
main(void)
{
    const char *path = "/tmp/probe_slab.mat";
    size_t cdims[2] = {1, 1};
    size_t dims[3] = {5, 1, 1};
    double vals[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const int start[3] = {1, 0, 0};
    const int stride[3] = {2, 1, 1};
    const int edge[3] = {2, 1, 1};
    mat_t *mat;
    matvar_t *cell, *inner, *info, *elem;
    double *out;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 0;
    cell = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 2, cdims, NULL, 0);
    inner = Mat_VarCreate(NULL, MAT_C_DOUBLE, MAT_T_DOUBLE, 3, dims, vals, 0);
    if ( cell == NULL || inner == NULL ) {
        Mat_VarFree(inner);
        Mat_VarFree(cell);
        Mat_Close(mat);
        return 0;
    }
    Mat_VarSetCell(cell, 0, inner);
    if ( Mat_VarWrite(mat, cell, MAT_COMPRESSION_ZLIB) != 0 ) {
        Mat_VarFree(cell);
        Mat_Close(mat);
        return 0;
    }
    Mat_VarFree(cell);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 0;
    info = Mat_VarReadInfo(mat, "c");
    if ( info == NULL ) {
        Mat_Close(mat);
        return 0;
    }
    elem = Mat_VarGetCell(info, 0);
    out = (double *)calloc(2, sizeof(double));
    if ( elem != NULL && out != NULL )
        (void)Mat_VarReadData(mat, elem, out, start, stride, edge);
    free(out);
    Mat_VarFree(info);
    Mat_Close(mat);
    return 0;
}
