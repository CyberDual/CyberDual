#define _POSIX_C_SOURCE 200809L

#include <matio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char path[] = "/tmp/matio-sparse-XXXXXX";
    const int32_t header[5] = {2, 2, 3, 0, 2};
    const char name[2] = {'s', '\0'};
    const double values[6] = {1, 3, 1, 16, 2.5, 0};
    mat_t *mat;
    matvar_t *var;
    FILE *fp;
    int fd = mkstemp(path);

    if ( fd < 0 )
        return 2;
    fp = fdopen(fd, "wb");
    if ( fp == NULL ) {
        close(fd);
        unlink(path);
        return 2;
    }
    if ( fwrite(header, sizeof(header), 1, fp) != 1 ||
         fwrite(name, sizeof(name), 1, fp) != 1 ||
         fwrite(values, sizeof(values), 1, fp) != 1 ) {
        fclose(fp);
        unlink(path);
        return 2;
    }
    fclose(fp);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        unlink(path);
        return 3;
    }
    var = Mat_VarReadNext(mat);
    if ( var != NULL )
        Mat_VarFree(var);
    Mat_Close(mat);
    unlink(path);

    return var == NULL ? 3 : 0;
}
