#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct input {
    unsigned char blocks[2560];
    unsigned char *current;
    size_t offset;
};

static void octal(unsigned char *field, size_t width, unsigned value)
{
    snprintf((char *)field, width, "%0*o", (int)width - 1, value);
}

static la_ssize_t read_block(struct archive *a, void *opaque, const void **buffer)
{
    struct input *in = opaque;
    unsigned char *next;
    (void)a;
    free(in->current);
    in->current = NULL;
    if (in->offset == sizeof(in->blocks)) {
        *buffer = NULL;
        return 0;
    }
    next = malloc(512);
    if (next == NULL)
        return -1;
    memcpy(next, in->blocks + in->offset, 512);
    in->current = next;
    in->offset += 512;
    *buffer = next;
    return 512;
}

int main(void)
{
    struct input in = {0};
    struct archive *a;
    struct archive_entry *entry;
    unsigned sum = 0;
    size_t i;
    int result;

    memcpy(in.blocks, "sparse", 6);
    octal(in.blocks + 100, 8, 0644);
    octal(in.blocks + 108, 8, 123);
    octal(in.blocks + 116, 8, 456);
    octal(in.blocks + 124, 12, 1);
    octal(in.blocks + 136, 12, 123456789);
    memset(in.blocks + 148, ' ', 8);
    in.blocks[156] = 'S';
    memcpy(in.blocks + 257, "ustar  ", 7);
    memcpy(in.blocks + 265, "user", 4);
    memcpy(in.blocks + 297, "group", 5);
    octal(in.blocks + 386, 12, 0);
    octal(in.blocks + 398, 12, 1);
    in.blocks[482] = '1';
    octal(in.blocks + 483, 12, 1);
    in.blocks[1024] = 'X';
    for (i = 0; i < 512; ++i)
        sum += in.blocks[i];
    octal(in.blocks + 148, 8, sum);

    a = archive_read_new();
    if (a == NULL)
        return 2;
    if (archive_read_support_format_tar(a) != ARCHIVE_OK)
        return 2;
    if (archive_read_open(a, &in, NULL, read_block, NULL) != ARCHIVE_OK)
        return 2;
    result = archive_read_next_header(a, &entry);
    if (result == ARCHIVE_OK)
        printf("%s\n", archive_entry_pathname(entry));
    else
        fprintf(stderr, "read failed: %s\n", archive_error_string(a));
    archive_read_free(a);
    free(in.current);
    return result == ARCHIVE_OK ? 0 : 1;
}
