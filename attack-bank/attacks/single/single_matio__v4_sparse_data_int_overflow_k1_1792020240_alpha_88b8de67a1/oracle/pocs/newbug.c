#include "matio.h"

#include <stdio.h>
#include <stdint.h>

static int
write_u32(FILE *fp, uint32_t value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1 ? 0 : -1;
}

static int
write_f64(FILE *fp, double value)
{
    return fwrite(&value, sizeof(value), 1, fp) == 1 ? 0 : -1;
}

int
main(void)
{
    static const char filename[] = "/tmp/matio-newbug.mat";
    static const char name[] = "x";
    FILE *fp = fopen(filename, "wb");
    mat_t *mat;
    matvar_t *var;

    if ( fp == NULL )
        return 1;

    if ( write_u32(fp, 2) || write_u32(fp, 2) || write_u32(fp, 3) ||
         write_u32(fp, 0) || write_u32(fp, 2) ||
         fwrite(name, 1, sizeof(name), fp) != sizeof(name) ||
         write_f64(fp, 1.0) || write_f64(fp, 1.0) || write_f64(fp, 1.0) ||
         write_f64(fp, 1073741823.0) || write_f64(fp, 1.0) ||
         write_f64(fp, 0.0) ) {
        fclose(fp);
        return 1;
    }

    if ( fclose(fp) != 0 )
        return 1;

    mat = Mat_Open(filename, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;

    var = Mat_VarReadNext(mat);
    if ( var != NULL )
        Mat_VarFree(var);
    Mat_Close(mat);

    return 0;
}
