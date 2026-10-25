/* Reads a crafted v5 MAT file containing a struct variable whose
 * "EnumerationInstanceTag" field is a 1x2 uint32 array with both words
 * equal to the MCOS reference value (0xDD000000).  The file is written
 * with the public matio write API and then read back through
 * Mat_Open + Mat_VarReadNext, the same entry points a user application
 * would use on an untrusted .mat file.
 */
#include <stdio.h>
#include <stdlib.h>
#include "matio.h"

#define MCOS_REF_VALUE 0xDD000000U

int
main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "enum_tag_multiword.mat";
    mat_t *mat;
    matvar_t *s, *tagv, *val, *r;
    size_t dims[2] = {1, 1};
    size_t tdims[2] = {1, 2};
    size_t vdims[2] = {1, 1};
    mat_uint32_t tag[2] = {MCOS_REF_VALUE, MCOS_REF_VALUE};
    double v = 1.0;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;

    s = Mat_VarCreate("e", MAT_C_STRUCT, MAT_T_STRUCT, 2, dims, NULL, 0);
    if ( s == NULL )
        return 2;
    if ( Mat_VarAddStructField(s, "EnumerationInstanceTag") ||
         Mat_VarAddStructField(s, "Values") )
        return 2;

    tagv = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, tdims, tag, 0);
    if ( tagv == NULL )
        return 2;
    Mat_VarSetStructFieldByName(s, "EnumerationInstanceTag", 0, tagv);

    val = Mat_VarCreate("Values", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, vdims, &v, 0);
    if ( val == NULL )
        return 2;
    Mat_VarSetStructFieldByName(s, "Values", 0, val);

    if ( Mat_VarWrite(mat, s, MAT_COMPRESSION_NONE) )
        return 2;
    Mat_VarFree(s);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 2;

    while ( (r = Mat_VarReadNext(mat)) != NULL )
        Mat_VarFree(r);

    Mat_Close(mat);
    return 0;
}
