#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct input {
    unsigned char bytes[6 * 512];
    size_t position;
    void *blocks[6];
    size_t count;
};

static void octal(unsigned char *dst, size_t length, unsigned value)
{
    snprintf((char *)dst, length, "%0*o", (int)length - 1, value);
}

static int open_cb(struct archive *a, void *client)
{
    (void)a;
    (void)client;
    return ARCHIVE_OK;
}

static la_ssize_t read_cb(struct archive *a, void *client, const void **buffer)
{
    struct input *in = client;
    void *block;
    (void)a;
    if (in->position == sizeof(in->bytes))
        return 0;
    block = malloc(512);
    if (block == NULL)
        return -1;
    memcpy(block, in->bytes + in->position, 512);
    in->blocks[in->count++] = block;
    in->position += 512;
    *buffer = block;
    return 512;
}

static la_int64_t skip_cb(struct archive *a, void *client, la_int64_t request)
{
    (void)a;
    (void)client;
    (void)request;
    return 0;
}

static int close_cb(struct archive *a, void *client)
{
    struct input *in = client;
    size_t i;
    (void)a;
    for (i = 0; i < in->count; ++i)
        free(in->blocks[i]);
    return ARCHIVE_OK;
}

int main(void)
{
    struct input in = {0};
    struct archive *a;
    struct archive_entry *entry;
    unsigned char *h = in.bytes;
    unsigned char *ext = in.bytes + 512;
    unsigned sum = 0;
    size_t i;
    int result;

    memset(h, 'n', 100);
    octal(h + 100, 8, 0644);
    octal(h + 108, 8, 0);
    octal(h + 116, 8, 0);
    octal(h + 124, 12, 22);
    octal(h + 136, 12, 0);
    memset(h + 148, ' ', 8);
    h[156] = 'S';
    memcpy(h + 257, "ustar  ", 7);
    octal(h + 386, 12, 1);
    octal(h + 398, 12, 1);
    h[482] = '1';
    octal(h + 483, 12, 23);
    for (i = 0; i < 512; ++i)
        sum += h[i];
    snprintf((char *)h + 148, 8, "%06o", sum);
    h[154] = '\0';
    h[155] = ' ';

    for (i = 0; i < 21; ++i) {
        octal(ext + i * 24, 12, (unsigned)i + 2);
        octal(ext + i * 24 + 12, 12, 1);
    }
    ext[504] = '1';
    memset(in.bytes + 3 * 512, 'x', 22);

    a = archive_read_new();
    if (a == NULL)
        return 2;
    archive_read_support_format_tar(a);
    result = archive_read_open2(a, &in, open_cb, read_cb, skip_cb, close_cb);
    if (result == ARCHIVE_OK)
        result = archive_read_next_header(a, &entry);
    archive_read_free(a);
    return result == ARCHIVE_OK ? 0 : 1;
}
