#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-utf8-XXXXXX";
    const size_t dims[2] = {1, 1};
    const unsigned char data[4] = {0xf0, 0x9f, 0x98, 0x80};
    mat_t *mat;
    matvar_t *var;
    int fd = mkstemp(path);

    if ( fd < 0 )
        return 1;
    close(fd);
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;
    var = Mat_VarCreate("a", MAT_C_CHAR, MAT_T_UTF8, 2, dims, (void *)data,
                        MAT_F_DONT_COPY_DATA);
    if ( var == NULL || Mat_VarWrite(mat, var, MAT_COMPRESSION_NONE) )
        return 3;
    Mat_VarFree(var);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 4;
    var = Mat_VarReadNext(mat);
    if ( var == NULL )
        return 5;
    Mat_VarFree(var);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
