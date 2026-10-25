#include "matio.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/matio_poc_newbug.mat";
    size_t dims[2] = {1, 1};
    mat_uint32_t tag_value = 0;
    matvar_t *tag, *st, *matvar;
    matvar_t *fields[2];
    mat_t *mat;

    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, dims, &tag_value,
                        0);
    if ( tag == NULL ) {
        fprintf(stderr, "failed to create tag variable\n");
        return EXIT_FAILURE;
    }

    fields[0] = tag;
    fields[1] = NULL;
    st = Mat_VarCreate("s", MAT_C_STRUCT, MAT_T_STRUCT, 2, dims, fields, 0);
    if ( st == NULL ) {
        Mat_VarFree(tag);
        fprintf(stderr, "failed to create struct variable\n");
        return EXIT_FAILURE;
    }

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL ) {
        Mat_VarFree(st);
        fprintf(stderr, "failed to create %s\n", path);
        return EXIT_FAILURE;
    }

    if ( Mat_VarWrite(mat, st, MAT_COMPRESSION_NONE) != MATIO_E_NO_ERROR ) {
        Mat_VarFree(st);
        Mat_Close(mat);
        fprintf(stderr, "failed to write struct variable\n");
        return EXIT_FAILURE;
    }
    Mat_VarFree(st);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "failed to open %s\n", path);
        return EXIT_FAILURE;
    }

    while ( NULL != (matvar = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);

    return EXIT_SUCCESS;
}
