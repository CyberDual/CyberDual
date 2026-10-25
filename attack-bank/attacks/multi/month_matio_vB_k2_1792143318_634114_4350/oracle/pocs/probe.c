#include <stdio.h>
#include <string.h>
#include <matio.h>

/*
 * Uncompressed little-endian MAT5 file: one 1x1 miUTF8 char array named "c"
 * whose single data byte is 0xFF. Structurally a normal character array; the
 * byte is not a continuation byte and does not start a 2/3/4-byte sequence.
 */
static const unsigned char payload[] = {
    0x0e, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int
main(void)
{
    unsigned char hdr[128];
    FILE *fp;
    mat_t *mat;
    matvar_t *matvar;
    const char *path = "/tmp/matio-utf8-char.mat";

    memset(hdr, ' ', sizeof(hdr));
    memcpy(hdr, "MATLAB 5.0 MAT-file, Platform: posix, Created by: libmatio", 59);
    hdr[124] = 0x00;
    hdr[125] = 0x01;
    hdr[126] = 'I';
    hdr[127] = 'M';

    fp = fopen(path, "wb");
    if ( fp == NULL )
        return 1;
    if ( fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
         fwrite(payload, 1, sizeof(payload), fp) != sizeof(payload) ) {
        fclose(fp);
        return 1;
    }
    fclose(fp);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;
    matvar = Mat_VarRead(mat, "c");
    if ( matvar != NULL && matvar->data != NULL && matvar->class_type == MAT_C_CHAR &&
         matvar->data_type == MAT_T_UTF8 && matvar->dims != NULL ) {
        matvar_t *copy = Mat_VarCreate(matvar->name, matvar->class_type, matvar->data_type,
                                        matvar->rank, matvar->dims, matvar->data, 0);
        Mat_VarFree(copy);
    }
    Mat_VarFree(matvar);
    Mat_Close(mat);
    remove(path);
    return 0;
}
