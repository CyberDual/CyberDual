#include <matio.h>
#include <stdlib.h>

int
main(void)
{
    size_t dims[2] = {4, 1};
    size_t edims[2] = {1, 1};
    matvar_t *cell, *elem;
    matvar_t **got;
    double value = 1.0;
    int i;

    cell = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 2, dims, NULL, 0);
    if ( cell == NULL )
        return 0;

    for ( i = 0; i < 4; i++ ) {
        elem = Mat_VarCreate("e", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, edims, &value, 0);
        if ( elem == NULL ) {
            Mat_VarFree(cell);
            return 0;
        }
        Mat_VarSetCell(cell, i, elem);
    }

    got = Mat_VarGetCellsLinear(cell, 4, 1, 1);
    if ( got != NULL )
        free(got);
    Mat_VarFree(cell);
    return 0;
}
