#include <hdf5.h>
#include <unistd.h>
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
    char filename[] = "/tmp/matio-residual-XXXXXX";
    char header[128] = {0};
    char values[32][8];
    hsize_t dataset_dims[1] = {1};
    hsize_t attribute_dims[1] = {32};
    hid_t file_id, fcpl_id, space_id, dataset_id, attribute_space_id;
    hid_t type_id, attribute_id;
    mat_t *mat;
    matvar_t *matvar;
    int fd;
    FILE *file;

    fd = mkstemp(filename);
    if ( fd < 0 ) {
        return 1;
    }
    close(fd);
    remove(filename);

    fcpl_id = H5Pcreate(H5P_FILE_CREATE);
    if ( fcpl_id < 0 || H5Pset_userblock(fcpl_id, 512) < 0 ) {
        return 1;
    }

    file_id = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl_id, H5P_DEFAULT);
    H5Pclose(fcpl_id);
    if ( file_id < 0 ) {
        remove(filename);
        return 1;
    }

    space_id = H5Screate_simple(1, dataset_dims, dataset_dims);
    dataset_id = H5Dcreate2(file_id, "x", H5T_NATIVE_DOUBLE, space_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Sclose(space_id);
    if ( dataset_id < 0 ) {
        H5Fclose(file_id);
        remove(filename);
        return 1;
    }

    type_id = H5Tcopy(H5T_C_S1);
    attribute_space_id = H5Screate_simple(1, attribute_dims, attribute_dims);
    if ( type_id < 0 || attribute_space_id < 0 || H5Tset_size(type_id, 8) < 0 ) {
        return 1;
    }

    memset(values, 0, sizeof(values));
    for ( size_t i = 0; i < 32; i++ ) {
        memcpy(values[i], "double", 6);
    }

    attribute_id = H5Acreate2(dataset_id, "MATLAB_class", type_id, attribute_space_id, H5P_DEFAULT, H5P_DEFAULT);
    if ( attribute_id < 0 || H5Awrite(attribute_id, type_id, values) < 0 ) {
        return 1;
    }

    H5Aclose(attribute_id);
    H5Sclose(attribute_space_id);
    H5Tclose(type_id);
    H5Dclose(dataset_id);
    H5Fclose(file_id);

    snprintf(header, sizeof(header), "MATLAB 7.3 MAT-file, Platform: GLNXA64, Created by: residual");
    header[124] = 0x00;
    header[125] = 0x02;
    header[126] = 'I';
    header[127] = 'M';

    file = fopen(filename, "r+b");
    if ( NULL == file || fwrite(header, 1, sizeof(header), file) != sizeof(header) ) {
        remove(filename);
        return 1;
    }
    fclose(file);

    mat = Mat_Open(filename, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        remove(filename);
        return 1;
    }

    matvar = Mat_VarReadNext(mat);
    Mat_VarFree(matvar);
    Mat_Close(mat);
    remove(filename);
    return 0;
}
