#include <stdint.h>
#include <stdio.h>
#include <matio.h>

int
main(void)
{
    const char *path = "/tmp/enum_tag_vec.mat";
    size_t dims[2] = {1, 1};
    size_t fdims[2] = {1, 2};
    uint32_t vals[2] = {0xDD000000u, 0};
    const char *fields[] = {"EnumerationInstanceTag", NULL};
    mat_t *mat;
    matvar_t *st;
    matvar_t *tag;
    matvar_t *got;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( mat == NULL )
        return 2;

    st = Mat_VarCreateStruct2("e", 2, dims, fields);
    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, fdims, vals, 0);
    if ( st == NULL || tag == NULL )
        return 2;
    Mat_VarSetStructFieldByName(st, "EnumerationInstanceTag", 0, tag);
    if ( Mat_VarWrite(mat, st, MAT_COMPRESSION_NONE) != 0 )
        return 2;
    Mat_VarFree(st);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 2;
    got = Mat_VarReadNext(mat);
    if ( got != NULL )
        Mat_VarFree(got);
    Mat_Close(mat);
    return 0;
}
