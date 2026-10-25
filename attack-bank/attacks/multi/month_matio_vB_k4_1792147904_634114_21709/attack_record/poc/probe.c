#include <matio.h>
#include <stdlib.h>

int
main(void)
{
    const char *path = "/tmp/poc_slab.mat";
    size_t dims[3] = {2, 2, 4};
    double data[16];
    size_t sdims[2] = {1, 1};
    const char *fields[2] = {"x", NULL};
    int start[3] = {1, 1, 3};
    int stride[3] = {1, 1, 1};
    int edge[3] = {1, 1, 1};
    double out = 0;
    mat_t *mat;
    matvar_t *var, *strct, *info, *field;
    int i, err;

    for ( i = 0; i < 16; i++ )
        data[i] = (double)(i + 1);

    var = Mat_VarCreate("x", MAT_C_DOUBLE, MAT_T_DOUBLE, 3, dims, data, 0);
    strct = Mat_VarCreateStruct2("s", 2, sdims, fields);
    if ( var == NULL || strct == NULL )
        return 1;
    Mat_VarSetStructFieldByName(strct, "x", 0, var);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 1;
    err = Mat_VarWrite(mat, strct, MAT_COMPRESSION_ZLIB);
    Mat_Close(mat);
    Mat_VarFree(strct);
    if ( err )
        return 1;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;
    info = Mat_VarReadInfo(mat, "s");
    if ( info == NULL ) {
        Mat_Close(mat);
        return 1;
    }
    field = Mat_VarGetStructFieldByName(info, "x", 0);
    if ( field == NULL ) {
        Mat_VarFree(info);
        Mat_Close(mat);
        return 1;
    }
    err = Mat_VarReadData(mat, field, &out, start, stride, edge);
    Mat_VarFree(info);
    Mat_Close(mat);
    if ( err || out != 16.0 )
        return 1;
    return 0;
}
