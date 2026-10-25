#include "matio.h"

#include <stddef.h>
#include <stdio.h>

int
main(void)
{
    const size_t dims[2] = {1, 1};
    const unsigned char data[3] = {0xE2, 0x82, 0xAC};
    matvar_t *variable = Mat_VarCreate("x", MAT_C_CHAR, MAT_T_UTF8, 2, dims, data, 0);
    mat_t *file = Mat_CreateVer("fixture.mat", NULL, MAT_FT_MAT5);
    if (file == NULL || variable == NULL)
        return 1;
    Mat_VarWrite(file, variable, MAT_COMPRESSION_NONE);
    Mat_VarFree(variable);
    Mat_Close(file);
    return 0;
}
