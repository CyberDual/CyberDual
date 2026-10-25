#include <matio.h>

#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char path[] = "/tmp/matio-rank-XXXXXX";
    size_t struct_dims[2] = {1, 1};
    size_t data_dims[11] = {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int start[11] = {0};
    int stride[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int edge[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    unsigned char values[2] = {0x31, 0x32};
    unsigned char output = 0;
    const char *fields[2] = {"value", NULL};
    mat_t *mat;
    matvar_t *container, *value, *read_container, *read_value;
    int fd = mkstemp(path);
    int rc;

    if ( fd < 0 )
        return 2;
    close(fd);
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 3;
    container = Mat_VarCreateStruct2("item", 2, struct_dims, fields);
    value = Mat_VarCreate("value", MAT_C_UINT8, MAT_T_UINT8, 11, data_dims, values, 0);
    if ( container == NULL || value == NULL )
        return 4;
    Mat_VarSetStructFieldByName(container, "value", 0, value);
    rc = Mat_VarWrite(mat, container, MAT_COMPRESSION_ZLIB);
    Mat_VarFree(container);
    Mat_Close(mat);
    if ( rc )
        return 5;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 6;
    read_container = Mat_VarReadInfo(mat, "item");
    if ( read_container == NULL )
        return 7;
    read_value = Mat_VarGetStructFieldByName(read_container, "value", 0);
    if ( read_value == NULL )
        return 8;
    rc = Mat_VarReadData(mat, read_value, &output, start, stride, edge);
    Mat_VarFree(read_container);
    Mat_Close(mat);
    unlink(path);
    return rc == 0 && output != values[0];
}
