#include <matio.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-slab-XXXXXX";
    size_t dims[11];
    int start[11], stride[11], edge[11];
    unsigned char value = 42, output = 0;
    mat_t *mat;
    matvar_t *var;
    int fd, i;

    for ( i = 0; i < 11; i++ ) {
        dims[i] = 1;
        start[i] = 0;
        stride[i] = 1;
        edge[i] = 1;
    }

    fd = mkstemp(path);
    if ( fd < 0 )
        return 1;
    close(fd);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;
    var = Mat_VarCreate("v", MAT_C_UINT8, MAT_T_UINT8, 11, dims, &value, 0);
    if ( var == NULL )
        return 3;
    if ( Mat_VarWrite(mat, var, MAT_COMPRESSION_NONE) != 0 )
        return 4;
    Mat_VarFree(var);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 5;
    var = Mat_VarReadInfo(mat, "v");
    if ( var == NULL )
        return 6;
    (void)Mat_VarReadData(mat, var, &output, start, stride, edge);
    Mat_VarFree(var);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
