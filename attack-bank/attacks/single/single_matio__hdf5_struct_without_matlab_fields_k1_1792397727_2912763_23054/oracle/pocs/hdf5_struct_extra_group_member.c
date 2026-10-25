/* Probe: read an HDF5 (MAT 7.3) struct that has no MATLAB_fields attribute.
 *
 * The struct group /s holds two dataset fields ("a", "b") plus one committed
 * datatype link ("T"). A reader that enumerates fields from the group's links
 * must skip the non-dataset/non-group member and report exactly two fields.
 *
 * The crafted file is created with the HDF5 API, then read back through the
 * public matio API: Mat_Open + Mat_VarRead.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hdf5.h>
#include "matio.h"

static void
write_mat73_header(const char *path)
{
    FILE *fp = fopen(path, "r+b");
    unsigned char hdr[128];
    if ( NULL == fp )
        return;
    memset(hdr, ' ', sizeof hdr);
    memcpy(hdr, "MATLAB 7.3 MAT-file, Platform: GLNXA64, Created by: probe, HDF5 schema 0.5", 75);
    memset(hdr + 116, ' ', 8);
    hdr[124] = 0x00;
    hdr[125] = 0x02; /* version 0x0200 */
    hdr[126] = 'I';
    hdr[127] = 'M'; /* little-endian indicator */
    (void)fseek(fp, 0L, SEEK_SET);
    (void)fwrite(hdr, 1, sizeof hdr, fp);
    fclose(fp);
}

static void
add_matlab_class(hid_t obj, const char *class_name)
{
    hid_t space_id, type_id, attr_id;

    space_id = H5Screate(H5S_SCALAR);
    type_id = H5Tcopy(H5T_C_S1);
    (void)H5Tset_size(type_id, strlen(class_name));
    (void)H5Tset_strpad(type_id, H5T_STR_NULLTERM);
    attr_id = H5Acreate(obj, "MATLAB_class", type_id, space_id, H5P_DEFAULT, H5P_DEFAULT);
    (void)H5Awrite(attr_id, type_id, class_name);
    (void)H5Aclose(attr_id);
    (void)H5Tclose(type_id);
    (void)H5Sclose(space_id);
}

static void
add_double_field(hid_t grp, const char *name, double value)
{
    hid_t space_id, dset_id;
    hsize_t dims[2] = {1, 1};

    space_id = H5Screate_simple(2, dims, NULL);
    dset_id = H5Dcreate(grp, name, H5T_NATIVE_DOUBLE, space_id, H5P_DEFAULT, H5P_DEFAULT,
                        H5P_DEFAULT);
    (void)H5Dwrite(dset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
    add_matlab_class(dset_id, "double");
    (void)H5Dclose(dset_id);
    (void)H5Sclose(space_id);
}

static void
create_probe_file(const char *path)
{
    hid_t fcpl_id, fapl_id, file_id, grp_id, dtype_id;

    fcpl_id = H5Pcreate(H5P_FILE_CREATE);
    (void)H5Pset_userblock(fcpl_id, 512);
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
#if H5_VERSION_GE(1, 10, 2)
    (void)H5Pset_libver_bounds(fapl_id, H5F_LIBVER_EARLIEST, H5F_LIBVER_V18);
#endif
    file_id = H5Fcreate(path, H5F_ACC_TRUNC, fcpl_id, fapl_id);
    (void)H5Pclose(fcpl_id);
    (void)H5Pclose(fapl_id);

    /* struct variable: group with MATLAB_class "struct" and NO MATLAB_fields */
    grp_id = H5Gcreate(file_id, "/s", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    add_matlab_class(grp_id, "struct");

    /* two ordinary double fields */
    add_double_field(grp_id, "a", 1.0);
    add_double_field(grp_id, "b", 2.0);

    /* a committed datatype member of the struct group: present in the link
     * count and in link iteration, but it is not a struct field */
    dtype_id = H5Tcopy(H5T_NATIVE_INT);
    (void)H5Tcommit2(file_id, "/s/T", dtype_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    (void)H5Tclose(dtype_id);

    (void)H5Gclose(grp_id);
    (void)H5Fclose(file_id);

    write_mat73_header(path);
}

int
main(void)
{
    char path[256];
    mat_t *mat;
    matvar_t *var;

    (void)snprintf(path, sizeof path, "/tmp/probe_struct_no_fields_attr_%ld.mat", (long)getpid());

    create_probe_file(path);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        (void)remove(path);
        return 2;
    }

    var = Mat_VarRead(mat, "s");
    if ( NULL == var ) {
        fprintf(stderr, "Mat_VarRead failed\n");
        (void)Mat_Close(mat);
        (void)remove(path);
        return 3;
    }

    printf("struct s: num_fields=%d\n", Mat_VarGetNumberOfFields(var));
    if ( 2 != Mat_VarGetNumberOfFields(var) ) {
        fprintf(stderr, "unexpected field count\n");
        Mat_VarFree(var);
        (void)Mat_Close(mat);
        (void)remove(path);
        return 4;
    }

    Mat_VarFree(var);
    (void)Mat_Close(mat);
    (void)remove(path);
    printf("OK\n");
    return 0;
}
