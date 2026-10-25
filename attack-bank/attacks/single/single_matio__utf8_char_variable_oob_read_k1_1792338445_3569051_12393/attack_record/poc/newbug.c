#include <matio.h>
#include <stdio.h>

int
main(void)
{
    const char *filename = "/tmp/matio_newbug.mat";
    const size_t dims[1] = {1};
    mat_uint8_t data[1] = {'A'};
    mat_t *output;
    mat_t *input;
    matvar_t *variable;

    output = Mat_CreateVer(filename, NULL, MAT_FT_MAT5);
    if ( output == NULL )
        return 1;
    variable = Mat_VarCreate("a", MAT_C_CHAR, MAT_T_UTF8, 1, dims, data, 0);
    if ( variable == NULL || Mat_VarWrite(output, variable, MAT_COMPRESSION_NONE) ) {
        Mat_Close(output);
        return 1;
    }
    Mat_VarFree(variable);
    Mat_Close(output);

    FILE *file = fopen(filename, "r+b");
    if ( file == NULL || fseek(file, 184L, SEEK_SET) != 0 || fputc(0xF0, file) == EOF ||
         fclose(file) != 0 )
        return 1;

    input = Mat_Open(filename, MAT_ACC_RDONLY);
    if ( input == NULL )
        return 1;
    variable = Mat_VarReadNext(input);
    Mat_VarFree(variable);
    Mat_Close(input);

    return 0;
}
