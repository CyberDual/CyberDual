#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <hdf5.h>
#include "matio.h"

#define CLASS_LEN 256

static int
write_mat(const char *path)
{
    hid_t plist, pal, fid, space, dset, str_type, aspace, attr;
    hsize_t dims[2];
    double value;
    char *class_name;
    uint16_t version, endian;
    FILE *fp;
    char hdr[116];

    plist = H5Pcreate(H5P_FILE_CREATE);
    if ( plist < 0 )
        return 1;
    if ( H5Pset_userblock(plist, 512) < 0 )
        return 1;
    pal = H5Pcreate(H5P_FILE_ACCESS);
    if ( pal < 0 )
        return 1;
    H5Pset_libver_bounds(pal, H5F_LIBVER_EARLIEST, H5F_LIBVER_V18);
    fid = H5Fcreate(path, H5F_ACC_TRUNC, plist, pal);
    H5Pclose(plist);
    H5Pclose(pal);
    if ( fid < 0 )
        return 1;

    dims[0] = 1;
    dims[1] = 1;
    value = 1.0;
    space = H5Screate_simple(2, dims, NULL);
    dset = H5Dcreate2(fid, "/x", H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Sclose(space);
    if ( dset < 0 ) {
        H5Fclose(fid);
        return 1;
    }
    if ( H5Dwrite(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value) < 0 ) {
        H5Dclose(dset);
        H5Fclose(fid);
        return 1;
    }

    class_name = (char *)malloc(CLASS_LEN);
    if ( NULL == class_name ) {
        H5Dclose(dset);
        H5Fclose(fid);
        return 1;
    }
    memset(class_name, 'n', CLASS_LEN);

    str_type = H5Tcopy(H5T_C_S1);
    H5Tset_size(str_type, CLASS_LEN);
    H5Tset_strpad(str_type, H5T_STR_NULLPAD);
    aspace = H5Screate(H5S_SCALAR);
    attr = H5Acreate2(dset, "MATLAB_class", str_type, aspace, H5P_DEFAULT, H5P_DEFAULT);
    if ( attr < 0 || H5Awrite(attr, str_type, class_name) < 0 ) {
        free(class_name);
        if ( attr >= 0 )
            H5Aclose(attr);
        H5Sclose(aspace);
        H5Tclose(str_type);
        H5Dclose(dset);
        H5Fclose(fid);
        return 1;
    }
    H5Aclose(attr);
    H5Sclose(aspace);
    H5Tclose(str_type);
    free(class_name);
    H5Dclose(dset);
    H5Fclose(fid);

    fp = fopen(path, "r+b");
    if ( NULL == fp )
        return 1;
    memset(hdr, ' ', sizeof(hdr));
    memcpy(hdr, "MATLAB 7.3 MAT-file, Platform: local, Created by: libmatio", 59);
    if ( fseek(fp, 0, SEEK_SET) != 0 ) {
        fclose(fp);
        return 1;
    }
    fwrite(hdr, 1, 116, fp);
    fwrite("        ", 1, 8, fp);
    version = 0x0200;
    endian = 0x4d49;
    fwrite(&version, 2, 1, fp);
    fwrite(&endian, 2, 1, fp);
    fclose(fp);
    return 0;
}

static int
read_mat(const char *path)
{
    mat_t *mat;
    matvar_t *matvar;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat )
        return 1;
    while ( NULL != (matvar = Mat_VarReadNext(mat)) )
        Mat_VarFree(matvar);
    Mat_Close(mat);
    return 0;
}

int
main(void)
{
    const char *path = "/tmp/long_class.mat";

    if ( write_mat(path) != 0 )
        return 1;
    return read_mat(path);
}
