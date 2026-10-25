#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char path[] = "/tmp/matio-probe-XXXXXX";
    const char *fields[] = {"text", NULL};
    const char value[16] = "abcdefghijklmnop";
    const size_t one[2] = {1, 1};
    const size_t dims[2] = {4, 4};
    const int start[2] = {0, 0};
    const int stride[2] = {1, 1};
    const int edge[2] = {4, 1};
    char output[4];
    mat_t *mat;
    matvar_t *parent, *child, *read_parent, *read_child;
    int fd = mkstemp(path);
    if (fd < 0) return 1;
    close(fd);
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if (!mat) return 2;
    parent = Mat_VarCreateStruct2("s", 2, one, fields);
    child = Mat_VarCreate("text", MAT_C_UINT8, MAT_T_UINT8, 2, dims, (void *)value, 0);
    if (!parent || !child) return 3;
    Mat_VarSetStructFieldByName(parent, "text", 0, child);
    if (Mat_VarWrite(mat, parent, MAT_COMPRESSION_ZLIB)) return 4;
    Mat_VarFree(parent);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if (!mat) return 5;
    read_parent = Mat_VarReadInfo(mat, "s");
    if (!read_parent) return 6;
    read_child = Mat_VarGetStructFieldByName(read_parent, "text", 0);
    if (!read_child) return 7;
    if (Mat_VarReadData(mat, read_child, output, start, stride, edge)) return 8;
    if (memcmp(output, value, sizeof(output))) return 9;
    Mat_VarFree(read_parent);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
