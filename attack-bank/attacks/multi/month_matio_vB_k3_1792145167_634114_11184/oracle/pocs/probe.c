#include <stdio.h>
#include <stdlib.h>

#include "matio.h"

/* 3-D cell of two elements, read back through the public cell API. */
int
main(void)
{
    size_t dims[3] = {2, 1, 1};
    size_t scalar[2] = {1, 1};
    double a = 1.0, b = 2.0;
    matvar_t *cell, *ca, *cb, *got;
    int start[3] = {0, 0, 0};
    int stride[3] = {1, 1, 1};
    int edge[3] = {2, 1, 1};
    matvar_t **slab;

    ca = Mat_VarCreate("a", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, scalar, &a, 0);
    cb = Mat_VarCreate("b", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, scalar, &b, 0);
    cell = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 3, dims, NULL, 0);
    if ( ca == NULL || cb == NULL || cell == NULL )
        return 0;

    Mat_VarSetCell(cell, 0, ca);
    Mat_VarSetCell(cell, 1, cb);

    got = Mat_VarGetCell(cell, 1);
    if ( got != NULL && got->data != NULL )
        (void)*(double *)got->data;

    slab = Mat_VarGetCells(cell, start, stride, edge);
    if ( slab != NULL ) {
        if ( slab[1] != NULL && slab[1]->data != NULL )
            (void)*(double *)slab[1]->data;
        free(slab);
    }

    Mat_VarFree(cell);
    return 0;
}
