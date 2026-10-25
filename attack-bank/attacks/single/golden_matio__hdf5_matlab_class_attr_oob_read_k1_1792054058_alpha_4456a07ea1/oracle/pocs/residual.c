#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdf5.h>
#include <matio.h>

static int
write_base(const char *path)
{
    mat_t *mat;
    matvar_t *var;
    double value = 1.0;
    size_t dims[2] = {1, 1};
    int rc;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat )
        return -1;
    var = Mat_VarCreate("x", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &value, 0);
    if ( NULL == var ) {
        Mat_Close(mat);
        return -1;
    }
    rc = Mat_VarWrite(mat, var, MAT_COMPRESSION_NONE);
    Mat_VarFree(var);
    Mat_Close(mat);
    return rc;
}

/* MATLAB_class as two fixed-length strings on a simple dataspace. */
static int
rewrite_class(const char *path)
{
    hid_t fid, dset, type_id, space_id, attr_id;
    hsize_t dims[1] = {2};
    char buf[12];
    herr_t herr;

    fid = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( fid < 0 )
        return -1;
    dset = H5Dopen(fid, "/x", H5P_DEFAULT);
    if ( dset < 0 ) {
        H5Fclose(fid);
        return -1;
    }
    if ( H5Aexists(dset, "MATLAB_class") > 0 )
        H5Adelete(dset, "MATLAB_class");
    type_id = H5Tcopy(H5T_C_S1);
    H5Tset_size(type_id, 6);
    H5Tset_strpad(type_id, H5T_STR_NULLPAD);
    space_id = H5Screate_simple(1, dims, NULL);
    attr_id = H5Acreate(dset, "MATLAB_class", type_id, space_id, H5P_DEFAULT, H5P_DEFAULT);
    if ( attr_id < 0 ) {
        H5Sclose(space_id);
        H5Tclose(type_id);
        H5Dclose(dset);
        H5Fclose(fid);
        return -1;
    }
    memcpy(buf, "double", 6);
    memcpy(buf + 6, "double", 6);
    herr = H5Awrite(attr_id, type_id, buf);
    H5Aclose(attr_id);
    H5Sclose(space_id);
    H5Tclose(type_id);
    H5Dclose(dset);
    H5Fclose(fid);
    return herr < 0 ? -1 : 0;
}

int
main(void)
{
    const char *path = "/tmp/matlab_class_simple.mat";
    mat_t *mat;
    matvar_t *var;

    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);
    if ( write_base(path) || rewrite_class(path) ) {
        fprintf(stderr, "failed to prepare %s\n", path);
        return 1;
    }

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }
    var = Mat_VarReadNext(mat);
    if ( NULL != var )
        Mat_VarFree(var);
    Mat_Close(mat);
    return 0;
}
