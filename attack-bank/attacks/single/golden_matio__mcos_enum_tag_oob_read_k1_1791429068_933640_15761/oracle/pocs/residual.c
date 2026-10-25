#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-enum-XXXXXX";
    const char *fields[] = {"EnumerationInstanceTag", NULL};
    size_t struct_dims[] = {1, 1};
    size_t tag_dims[] = {0, 1};
    mat_t *mat;
    matvar_t *var;
    matvar_t *tag;
    int fd = mkstemp(path);

    if ( fd < 0 )
        return 1;
    close(fd);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;
    var = Mat_VarCreateStruct2("sample", 2, struct_dims, fields);
    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, tag_dims,
                        NULL, 0);
    if ( var == NULL || tag == NULL )
        return 3;
    if ( Mat_VarSetStructFieldByName(var, fields[0], 0, tag) != NULL )
        return 4;
    if ( Mat_VarWrite(mat, var, MAT_COMPRESSION_NONE) != 0 )
        return 5;
    Mat_VarFree(var);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 6;
    var = Mat_VarReadNext(mat);
    if ( var != NULL )
        Mat_VarFree(var);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
