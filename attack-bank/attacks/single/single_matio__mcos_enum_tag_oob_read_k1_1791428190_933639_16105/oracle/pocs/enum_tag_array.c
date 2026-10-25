#include "matio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-enum-tag-XXXXXX";
    const char *fields[] = {"EnumerationInstanceTag", NULL};
    size_t struct_dims[] = {1, 1};
    size_t tag_dims[] = {1, 2};
    uint32_t values[] = {0x12345678U, 0x9abcdef0U};
    mat_t *mat;
    matvar_t *record, *tag, *readback;
    int fd = mkstemp(path);

    if ( fd < 0 )
        return 1;
    close(fd);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;
    record = Mat_VarCreateStruct2("record", 2, struct_dims, fields);
    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, tag_dims,
                        values, 0);
    if ( record == NULL || tag == NULL )
        return 3;
    Mat_VarSetStructFieldByName(record, fields[0], 0, tag);
    if ( Mat_VarWrite(mat, record, MAT_COMPRESSION_NONE) != 0 )
        return 4;
    Mat_VarFree(record);
    Mat_Close(mat);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 5;
    readback = Mat_VarReadNext(mat);
    if ( readback == NULL )
        return 6;
    Mat_VarFree(readback);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
