#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdf5.h>
#include "matio.h"

static void
make_field_name(char *dst)
{
    memset(dst, 'x', 64);
    dst[0] = 'z';
    dst[64] = '\0';
}

static int
write_struct(const char *path, const char *field_b)
{
    mat_t *mat;
    matvar_t *s, *fa, *fb;
    size_t dims[2] = {1, 1};
    const char *fields[3];
    double va = 1.0;
    double vb = 2.5;
    int err;

    fields[0] = "a";
    fields[1] = field_b;
    fields[2] = NULL;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat )
        return -1;
    s = Mat_VarCreateStruct2("s", 2, dims, fields);
    fa = Mat_VarCreate("a", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &va, 0);
    fb = Mat_VarCreate(field_b, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &vb, 0);
    if ( NULL == s || NULL == fa || NULL == fb ) {
        Mat_VarFree(fa);
        Mat_VarFree(fb);
        Mat_VarFree(s);
        Mat_Close(mat);
        return -1;
    }
    Mat_VarSetStructFieldByName(s, "a", 0, fa);
    Mat_VarSetStructFieldByName(s, field_b, 0, fb);
    err = Mat_VarWrite(mat, s, MAT_COMPRESSION_NONE);
    Mat_VarFree(s);
    Mat_Close(mat);
    return err;
}

static int
strip_matlab_fields(const char *path)
{
    hid_t fid = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    herr_t herr;
    if ( fid < 0 )
        return -1;
    if ( H5Aexists_by_name(fid, "/s", "MATLAB_fields", H5P_DEFAULT) > 0 ) {
        herr = H5Adelete_by_name(fid, "/s", "MATLAB_fields", H5P_DEFAULT);
        if ( herr < 0 ) {
            H5Fclose(fid);
            return -1;
        }
    }
    H5Fclose(fid);
    return 0;
}

static int
read_struct(const char *path, const char *field_b)
{
    mat_t *mat;
    matvar_t *s, *fa, *fb;
    double *da, *db;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return -1;
    }
    s = Mat_VarRead(mat, "s");
    Mat_Close(mat);
    if ( NULL == s ) {
        fprintf(stderr, "Mat_VarRead failed\n");
        return -1;
    }
    fa = Mat_VarGetStructFieldByName(s, "a", 0);
    fb = Mat_VarGetStructFieldByName(s, field_b, 0);
    if ( Mat_VarGetNumberOfFields(s) != 2 || NULL == fa || NULL == fb || NULL == fa->data ||
         NULL == fb->data ) {
        fprintf(stderr, "struct fields were not read\n");
        Mat_VarFree(s);
        return -1;
    }
    da = (double *)fa->data;
    db = (double *)fb->data;
    if ( da[0] != 1.0 || db[0] != 2.5 ) {
        fprintf(stderr, "unexpected values %g %g\n", da[0], db[0]);
        Mat_VarFree(s);
        return -1;
    }
    printf("read struct s with %u fields\n", Mat_VarGetNumberOfFields(s));
    Mat_VarFree(s);
    return 0;
}

int
main(void)
{
    char field_b[65];
    const char *path = "/tmp/residual_nofields.mat";
    int err;

    make_field_name(field_b);
    remove(path);
    err = write_struct(path, field_b);
    if ( 0 != err ) {
        fprintf(stderr, "failed to write MAT file\n");
        return 1;
    }
    err = strip_matlab_fields(path);
    if ( 0 != err ) {
        fprintf(stderr, "failed to remove MATLAB_fields\n");
        return 1;
    }
    err = read_struct(path, field_b);
    remove(path);
    return 0 == err ? 0 : 1;
}
