#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    char path[] = "/tmp/matio-slab-XXXXXX";
    int fd = mkstemp(path);
    size_t cdims[2] = {1, 1};
    size_t ndims[2] = {2, 3};
    double values[6] = {1, 2, 3, 4, 5, 6};
    int start[2] = {0, 1};
    int stride[2] = {1, 1};
    int edge[2] = {2, 1};
    double out[2] = {0, 0};
    mat_t *mat;
    matvar_t *cell, *child, *read_cell, *read_child;
    int rc;

    if (fd < 0) return 1;
    close(fd);
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if (!mat) return 2;
    cell = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 2, cdims, NULL, 0);
    child = Mat_VarCreate("x", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, ndims, values, 0);
    if (!cell || !child) return 3;
    Mat_VarSetCell(cell, 0, child);
    rc = Mat_VarWrite(mat, cell, MAT_COMPRESSION_ZLIB);
    Mat_VarFree(cell);
    Mat_Close(mat);
    if (rc) return 4;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if (!mat) return 5;
    read_cell = Mat_VarReadInfo(mat, "c");
    if (!read_cell) return 6;
    read_child = Mat_VarGetCell(read_cell, 0);
    if (!read_child) return 7;
    rc = Mat_VarReadData(mat, read_child, out, start, stride, edge);
    fprintf(stderr, "rc=%d out=%g,%g\n", rc, out[0], out[1]);
    Mat_VarFree(read_cell);
    Mat_Close(mat);
    unlink(path);
    return rc != 0 || out[0] != 3 || out[1] != 4;
}
