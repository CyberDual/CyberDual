/* Builds a MAT5 file with a struct carrying a 0-element uint32
 * "EnumerationInstanceTag" field, then reads it back through the public API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <matio.h>

static void build_file(const char *path)
{
    mat_t *mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL ) { fprintf(stderr, "create failed\n"); exit(2); }

    size_t sdims[2] = {1, 1};
    matvar_t *st = Mat_VarCreateStruct2("s", 2, sdims, NULL);
    if ( st == NULL ) { fprintf(stderr, "struct create failed\n"); exit(2); }

    Mat_VarAddStructField(st, "EnumerationInstanceTag");

    /* 0-element uint32 field: dims {1,0} */
    size_t fdims[2] = {1, 0};
    matvar_t *tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32,
                                  MAT_T_UINT32, 2, fdims, NULL, 0);
    if ( tag == NULL ) { fprintf(stderr, "tag create failed\n"); exit(2); }
    Mat_VarSetStructFieldByName(st, "EnumerationInstanceTag", 0, tag);

    if ( Mat_VarWrite(mat, st, MAT_COMPRESSION_NONE) != 0 )
        fprintf(stderr, "write failed\n");
    Mat_VarFree(st);
    Mat_Close(mat);
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/tmp/poc_enum.mat";
    build_file(path);

    mat_t *mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) { fprintf(stderr, "open failed\n"); return 2; }

    matvar_t *v;
    while ( (v = Mat_VarReadNext(mat)) != NULL ) {
        fprintf(stderr, "read var: %s class=%d\n", v->name ? v->name : "(null)", v->class_type);
        Mat_VarFree(v);
    }
    Mat_Close(mat);
    fprintf(stderr, "done\n");
    return 0;
}
