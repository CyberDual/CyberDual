#include "matio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
write_mat(const char *path)
{
    FILE *fp;
    const int32_t type = 2;
    const int32_t mrows = 2;
    const int32_t ncols = 3;
    const int32_t imagf = 0;
    const int32_t namelen = 2;
    const char name[2] = {'a', '\0'};
    const double ir = 2.0;
    const double nrows = 1.0;
    const double col = 1.0;
    const double mcols = 1.0;
    const double value = 4.0;
    const double pad = 0.0;

    fp = fopen(path, "wb");
    if ( fp == NULL )
        return 1;
    if ( fwrite(&type, sizeof(type), 1, fp) != 1 ||
         fwrite(&mrows, sizeof(mrows), 1, fp) != 1 ||
         fwrite(&ncols, sizeof(ncols), 1, fp) != 1 ||
         fwrite(&imagf, sizeof(imagf), 1, fp) != 1 ||
         fwrite(&namelen, sizeof(namelen), 1, fp) != 1 ||
         fwrite(name, 1, sizeof(name), fp) != sizeof(name) ||
         fwrite(&ir, sizeof(ir), 1, fp) != 1 || fwrite(&nrows, sizeof(nrows), 1, fp) != 1 ||
         fwrite(&col, sizeof(col), 1, fp) != 1 || fwrite(&mcols, sizeof(mcols), 1, fp) != 1 ||
         fwrite(&value, sizeof(value), 1, fp) != 1 || fwrite(&pad, sizeof(pad), 1, fp) != 1 ) {
        fclose(fp);
        return 1;
    }
    if ( fclose(fp) != 0 )
        return 1;
    return 0;
}

int
main(void)
{
    mat_t *mat;
    matvar_t *matvar;
    const char *path = "/tmp/matio_sparse_v4.mat";

    if ( write_mat(path) != 0 )
        return 1;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;

    matvar = Mat_VarReadNext(mat);
    if ( matvar != NULL )
        Mat_VarFree(matvar);
    Mat_Close(mat);
    return 0;
}
