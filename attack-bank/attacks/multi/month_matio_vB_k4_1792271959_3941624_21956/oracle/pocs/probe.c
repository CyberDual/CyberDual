#include <stdio.h>
#include <stdlib.h>

#include <matio.h>

int
main(void)
{
    const char *path = "/tmp/matio-rank14.mat";
    const size_t dims[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const int start[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const int stride[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const int edge[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    double value = 42.0;
    double data = 0.0;
    mat_t *mat;
    matvar_t *variable;

    remove(path);
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 1;
    variable = Mat_VarCreate("rank14", MAT_C_DOUBLE, MAT_T_DOUBLE, 14, dims, &value, 0);
    if ( variable == NULL || Mat_VarWrite(mat, variable, MAT_COMPRESSION_ZLIB) != 0 ) {
        Mat_VarFree(variable);
        Mat_Close(mat);
        return 1;
    }
    Mat_VarFree(variable);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;
    variable = Mat_VarReadNextInfo(mat);
    if ( variable == NULL ) {
        Mat_Close(mat);
        return 0;
    }
    (void)Mat_VarReadData(mat, variable, &data, start, stride, edge);
    Mat_VarFree(variable);
    Mat_Close(mat);
    return 0;
}
