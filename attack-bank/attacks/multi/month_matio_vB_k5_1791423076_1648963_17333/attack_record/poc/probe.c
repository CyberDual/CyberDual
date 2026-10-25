#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char path[] = "/tmp/matio-probe-XXXXXX";
    int fd = mkstemp(path);
    size_t dims[2] = {2, 4};
    size_t one[2] = {1, 1};
    unsigned char values[8] = {0,1,2,3,4,5,6,7};
    matvar_t *child, *outer, *read_outer, *read_child;
    matvar_t *cells[1];
    mat_t *mat;
    int start[2] = {0, 2}, stride[2] = {1, 1}, edge[2] = {2, 2};
    unsigned char output[4] = {0};
    if (fd < 0) return 1;
    close(fd);
    child = Mat_VarCreate(NULL, MAT_C_UINT8, MAT_T_UINT8, 2, dims, values, 0);
    if (!child) return 2;
    cells[0] = child;
    outer = Mat_VarCreate("cell", MAT_C_CELL, MAT_T_CELL, 2, one, cells, 0);
    if (!outer) return 3;
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if (!mat) return 4;
    if (Mat_VarWrite(mat, outer, MAT_COMPRESSION_ZLIB)) return 5;
    Mat_Close(mat);
    Mat_VarFree(outer);
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if (!mat) return 6;
    read_outer = Mat_VarReadInfo(mat, "cell");
    if (!read_outer) return 7;
    read_child = Mat_VarGetCell(read_outer, 0);
    if (!read_child) return 8;
    int rc = Mat_VarReadData(mat, read_child, output, start, stride, edge);
    Mat_VarFree(read_outer);
    Mat_Close(mat);
    unlink(path);
    return rc != 0 || output[0] != 4 || output[1] != 5 || output[2] != 6 || output[3] != 7;
}
