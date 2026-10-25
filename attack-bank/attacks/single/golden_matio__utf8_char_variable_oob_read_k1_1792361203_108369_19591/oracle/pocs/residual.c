#include "matio.h"

#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    const size_t dims[2] = {1, 1};
    const unsigned char data[2] = {0xC2, 0x80};
    const unsigned int crafted_dims[2] = {2, 1};
    mat_t *file;
    matvar_t *variable;
    matvar_t *read_variable;
    matvar_t *copy;
    FILE *fp;

    remove("residual.mat");

    variable = Mat_VarCreate("x", MAT_C_CHAR, MAT_T_UTF8, 2, dims, data, 0);
    file = Mat_CreateVer("residual.mat", NULL, MAT_FT_MAT5);
    if ( variable == NULL || file == NULL ) {
        Mat_VarFree(variable);
        Mat_Close(file);
        return 1;
    }

    Mat_VarWrite(file, variable, MAT_COMPRESSION_NONE);
    Mat_VarFree(variable);
    Mat_Close(file);

    fp = fopen("residual.mat", "r+b");
    if ( fp == NULL )
        return 1;
    if ( fseek(fp, 160, SEEK_SET) != 0 ||
         fwrite(crafted_dims, sizeof(crafted_dims), 1, fp) != 1 ) {
        fclose(fp);
        return 1;
    }
    fclose(fp);

    file = Mat_Open("residual.mat", MAT_ACC_RDONLY);
    if ( file == NULL )
        return 1;

    read_variable = Mat_VarReadNext(file);
    if ( read_variable == NULL ) {
        Mat_Close(file);
        return 0;
    }

    copy = Mat_VarCreate(read_variable->name, read_variable->class_type,
                         read_variable->data_type, read_variable->rank, read_variable->dims,
                         read_variable->data, 0);
    Mat_VarFree(copy);
    Mat_VarFree(read_variable);
    Mat_Close(file);
    return 0;
}
