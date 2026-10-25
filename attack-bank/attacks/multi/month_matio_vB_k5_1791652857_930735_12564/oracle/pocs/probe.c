#include <matio.h>
#include <stddef.h>

int
main(void)
{
    size_t dims[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const char *fields[] = {"a", NULL};
    matvar_t *matvar = Mat_VarCreateStruct2("s", 10, dims, fields);
    if ( matvar == NULL )
        return 1;

    int start[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int stride[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int edge[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    matvar_t *slab = Mat_VarGetStructs(matvar, start, stride, edge, 0);

    Mat_VarFree(slab);
    Mat_VarFree(matvar);
    return 0;
}
