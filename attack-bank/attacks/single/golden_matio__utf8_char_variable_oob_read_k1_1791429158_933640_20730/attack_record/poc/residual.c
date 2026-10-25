#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-residual-XXXXXX";
    const unsigned char text[] = {0xC2, 0xA2};
    const unsigned char tag[] = {0x10, 0x00, 0x00, 0x00, 0x02,
                                 0x00, 0x00, 0x00, 0xC2, 0xA2};
    size_t dims[] = {1, 1};
    unsigned char *file = NULL;
    size_t size, i;
    mat_t *mat;
    matvar_t *var, *copy;
    FILE *fp;
    int fd = mkstemp(path);

    if ( fd < 0 )
        return 1;
    close(fd);

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 2;
    var = Mat_VarCreate("a", MAT_C_CHAR, MAT_T_UTF8, 2, dims, (void *)text, 0);
    if ( var == NULL || Mat_VarWrite(mat, var, MAT_COMPRESSION_NONE) != 0 )
        return 3;
    Mat_VarFree(var);
    Mat_Close(mat);

    fp = fopen(path, "r+b");
    if ( fp == NULL || fseek(fp, 0, SEEK_END) != 0 )
        return 4;
    size = (size_t)ftell(fp);
    file = malloc(size);
    if ( file == NULL || fseek(fp, 0, SEEK_SET) != 0 || fread(file, 1, size, fp) != size )
        return 5;
    for ( i = 0; i + sizeof(tag) <= size; i++ ) {
        if ( memcmp(file + i, tag, sizeof(tag)) == 0 )
            break;
    }
    if ( i + sizeof(tag) > size )
        return 6;
    file[i + 4] = 1;
    file[i + 9] = 0;
    if ( fseek(fp, 0, SEEK_SET) != 0 || fwrite(file, 1, size, fp) != size )
        return 7;
    free(file);
    fclose(fp);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 8;
    var = Mat_VarReadNext(mat);
    if ( var != NULL ) {
        copy = Mat_VarCreate("copy", var->class_type, var->data_type, var->rank, var->dims,
                             var->data, 0);
        Mat_VarFree(copy);
        Mat_VarFree(var);
    }
    Mat_Close(mat);
    unlink(path);
    return 0;
}
