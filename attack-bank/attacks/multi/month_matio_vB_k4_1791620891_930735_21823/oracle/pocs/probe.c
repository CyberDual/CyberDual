#include <stdio.h>
#include <stdlib.h>

#include "matio.h"

int
main(void)
{
    const char *fields[] = {"a", NULL};
    size_t dims[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int start[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int stride[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int edge[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    matvar_t *matvar;
    matvar_t *slab;

    matvar = Mat_VarCreateStruct2("s", 10, dims, fields);
    if ( NULL == matvar ) {
        fprintf(stderr, "Failed to create struct\n");
        return 1;
    }

    slab = Mat_VarGetStructs(matvar, start, stride, edge, 0);

    Mat_VarFree(slab);
    Mat_VarFree(matvar);

    printf("done\n");
    return 0;
}
