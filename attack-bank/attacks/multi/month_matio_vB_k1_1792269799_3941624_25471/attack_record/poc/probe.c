#include <stdlib.h>

#include "matio.h"

int
main(void)
{
    const size_t dims[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const char *fields[] = {"value", NULL};
    const int start[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const int stride[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const int edge[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const size_t scalar_dims[2] = {1, 1};
    const double value = 1.0;
    matvar_t *field;
    matvar_t *slab;
    matvar_t *var = Mat_VarCreateStruct2("probe", 10, dims, fields);
    if ( var == NULL )
        return EXIT_FAILURE;

    field = Mat_VarCreate("value", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, scalar_dims, &value, 0);
    if ( field == NULL ) {
        Mat_VarFree(var);
        return EXIT_FAILURE;
    }
    Mat_VarSetStructFieldByIndex(var, 0, 0, field);

    slab = Mat_VarGetStructs(var, start, stride, edge, 1);
    Mat_VarFree(slab);
    Mat_VarFree(var);
    return EXIT_SUCCESS;
}
