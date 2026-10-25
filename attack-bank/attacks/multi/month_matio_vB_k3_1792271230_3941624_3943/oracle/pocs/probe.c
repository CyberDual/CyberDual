#include <matio.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *path = "/tmp/matio-rank11.mat";
    const char *fields[] = {"value", NULL};
    size_t struct_dims[1] = {1};
    size_t value_dims[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int start[11] = {0};
    int stride[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int edge[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    double value = 1.0;
    double output = 0.0;
    mat_t *mat;
    matvar_t *structure;
    matvar_t *field;
    matvar_t *element;
    int err;

    mat = Mat_CreateVer(path, "rank-11 probe", MAT_FT_MAT5);
    if ( mat == NULL )
        return EXIT_FAILURE;

    element = Mat_VarCreate("value", MAT_C_DOUBLE, MAT_T_DOUBLE, 11, value_dims, &value, 0);
    if ( element == NULL ) {
        Mat_Close(mat);
        return EXIT_FAILURE;
    }

    structure = Mat_VarCreateStruct2("s", 1, struct_dims, fields);
    if ( structure == NULL ) {
        Mat_VarFree(element);
        Mat_Close(mat);
        return EXIT_FAILURE;
    }

    (void)Mat_VarSetStructFieldByName(structure, "value", 0, element);

    err = Mat_VarWrite(mat, structure, MAT_COMPRESSION_ZLIB);
    Mat_VarFree(structure);
    Mat_Close(mat);
    if ( err )
        return EXIT_FAILURE;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return EXIT_FAILURE;

    structure = Mat_VarReadInfo(mat, "s");
    if ( structure == NULL ) {
        Mat_Close(mat);
        return EXIT_FAILURE;
    }

    field = Mat_VarGetStructFieldByName(structure, "value", 0);
    if ( field == NULL || field->rank != 11 ) {
        Mat_VarFree(structure);
        Mat_Close(mat);
        return EXIT_FAILURE;
    }

    err = Mat_VarReadData(mat, field, &output, start, stride, edge);
    Mat_VarFree(structure);
    Mat_Close(mat);
    remove(path);

    return err == MATIO_E_NO_ERROR && output == 1.0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
