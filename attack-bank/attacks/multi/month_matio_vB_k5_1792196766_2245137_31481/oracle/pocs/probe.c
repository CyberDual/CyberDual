/* Probe: slab-read a compressed variable from a MAT v5 file through the
 * public matio API (Mat_Open + Mat_VarReadNextInfo + Mat_VarReadDataLinear).
 *
 * The file is a well-formed MAT5 file holding one small compressed double
 * variable "x" (dims [1 4], payload stored as miINT32).  The probe requests a
 * linear slab with a negative element count; a robust library must reject the
 * arguments with a clean error return.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <matio.h>

#include "mat_bytes.h"

/* Ask the AddressSanitizer runtime to terminate the process with SIGABRT
 * when it reports an error (standard ASan default-options hook). */
const char *
__asan_default_options(void)
{
    return "abort_on_error=1";
}

int
main(void)
{
    char path[] = "/tmp/matio_probe_XXXXXX";
    int fd = mkstemp(path);
    if ( fd < 0 ) {
        perror("mkstemp");
        return 2;
    }
    ssize_t wr = write(fd, mat_file, mat_file_len);
    (void)close(fd);
    if ( wr != (ssize_t)mat_file_len ) {
        perror("write");
        return 2;
    }

    mat_t *mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }

    matvar_t *var = Mat_VarReadNextInfo(mat);
    if ( var == NULL ) {
        fprintf(stderr, "Mat_VarReadNextInfo failed\n");
        Mat_Close(mat);
        return 2;
    }

    size_t nelems = (size_t)var->dims[0] * (size_t)var->dims[1];
    double *buf = (double *)malloc(nelems * sizeof(double));
    if ( buf == NULL ) {
        Mat_VarFree(var);
        Mat_Close(mat);
        return 2;
    }

    /* Negative edge count: library should return an error, not crash. */
    int err = Mat_VarReadDataLinear(mat, var, buf, 0, 1, -1);
    printf("Mat_VarReadDataLinear returned %d\n", err);

    free(buf);
    Mat_VarFree(var);
    Mat_Close(mat);
    unlink(path);
    return 0;
}
