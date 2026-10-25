#include <matio.h>
#include <stdio.h>

int
main(void)
{
    const char *path = "/tmp/matio-probe.mat";
    size_t cell_dims[3] = {1, 1, 1};
    size_t data_dims[3] = {2, 2, 8};
    double data[32] = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    };
    int start[3] = {0, 0, 7};
    int stride[3] = {1, 1, 1};
    int edge[3] = {2, 2, 1};
    double result[4];
    mat_t *mat;
    matvar_t *cell, *element, *slab_source;
    int err;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;

    slab_source = Mat_VarCreate("slab", MAT_C_DOUBLE, MAT_T_DOUBLE, 3, data_dims, data, 0);
    cell = Mat_VarCreate("cell", MAT_C_CELL, MAT_T_CELL, 3, cell_dims, NULL, 0);
    if ( slab_source == NULL || cell == NULL ) {
        Mat_VarFree(slab_source);
        Mat_VarFree(cell);
        Mat_Close(mat);
        return 3;
    }

    Mat_VarSetCell(cell, 0, slab_source);
    err = Mat_VarWrite(mat, cell, MAT_COMPRESSION_ZLIB);
    Mat_VarFree(cell);
    Mat_Close(mat);
    if ( err )
        return 4;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 5;

    cell = Mat_VarReadInfo(mat, "cell");
    if ( cell == NULL ) {
        Mat_Close(mat);
        return 6;
    }

    element = Mat_VarGetCell(cell, 0);
    if ( element == NULL ) {
        Mat_VarFree(cell);
        Mat_Close(mat);
        return 7;
    }

    err = Mat_VarReadData(mat, element, result, start, stride, edge);
    printf("read %g %g %g %g\n", result[0], result[1], result[2], result[3]);

    Mat_VarFree(cell);
    Mat_Close(mat);
    return err;
}
