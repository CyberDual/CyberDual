/* Probe: builds a crafted MAT v7.3 (HDF5) file whose MATLAB_class attribute
 * is a fixed-length string of exactly 64 bytes, then reads it back through
 * the public matio API (Mat_Open + Mat_VarReadNext).
 *
 * Usage: h5_class_len64 [path-to-mat-file]
 */
#include <stdio.h>
#include <string.h>
#include <hdf5.h>
#include <matio.h>

int
main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "h5_class_len64.mat";
    hid_t fcpl_id, fid, space_id, dset_id, aspace_id, attr_id, atype_id;
    hsize_t dims[2] = {2, 2};
    double data[4] = {1.0, 2.0, 3.0, 4.0};
    char class_name[64];
    char header[116];
    char subsys[8];
    mat_int16_t version = 0x0200;
    mat_int16_t endian = 0x4d49;
    FILE *fp;
    mat_t *mat;
    matvar_t *matvar;

    memset(class_name, 'A', sizeof(class_name));

    /* HDF5 container with a 512-byte user block, like Mat_Create73 */
    fcpl_id = H5Pcreate(H5P_FILE_CREATE);
    H5Pset_userblock(fcpl_id, 512);
    fid = H5Fcreate(path, H5F_ACC_TRUNC, fcpl_id, H5P_DEFAULT);
    H5Pclose(fcpl_id);
    if ( fid < 0 ) {
        fprintf(stderr, "H5Fcreate failed\n");
        return 1;
    }

    /* Plain 2x2 double dataset */
    space_id = H5Screate_simple(2, dims, NULL);
    dset_id = H5Dcreate(fid, "x", H5T_IEEE_F64LE, space_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Dwrite(dset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    /* MATLAB_class attribute: scalar, fixed-length string of 64 bytes */
    atype_id = H5Tcopy(H5T_C_S1);
    H5Tset_size(atype_id, sizeof(class_name));
    H5Tset_strpad(atype_id, H5T_STR_NULLPAD);
    aspace_id = H5Screate(H5S_SCALAR);
    attr_id = H5Acreate(dset_id, "MATLAB_class", atype_id, aspace_id, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, atype_id, class_name);
    H5Aclose(attr_id);
    H5Sclose(aspace_id);
    H5Tclose(atype_id);
    H5Dclose(dset_id);
    H5Sclose(space_id);
    H5Fclose(fid);

    /* 128-byte MAT v7.3 header at the start of the user block */
    memset(header, ' ', sizeof(header));
    snprintf(header, sizeof(header),
             "MATLAB 7.3 MAT-file, Platform: GLNXA64, HDF5 schema 0.5");
    memset(subsys, ' ', sizeof(subsys));
    fp = fopen(path, "r+b");
    if ( NULL == fp ) {
        fprintf(stderr, "fopen failed\n");
        return 1;
    }
    fwrite(header, 1, sizeof(header), fp);
    fwrite(subsys, 1, sizeof(subsys), fp);
    fwrite(&version, 2, 1, fp);
    fwrite(&endian, 2, 1, fp);
    fclose(fp);

    /* Read the crafted file back through the public API */
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }
    while ( NULL != (matvar = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);

    return 0;
}
