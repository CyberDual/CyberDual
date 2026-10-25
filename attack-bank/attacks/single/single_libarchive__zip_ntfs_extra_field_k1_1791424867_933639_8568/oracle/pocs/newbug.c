#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put16(unsigned char *p, unsigned value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put32(unsigned char *p, uint32_t value)
{
    put16(p, value);
    put16(p + 2, value >> 16);
}

static void put64(unsigned char *p, uint64_t value)
{
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

static void extra(unsigned char *p)
{
    put16(p, 0x000a);
    put16(p + 2, 36);
    put32(p + 4, 0);
    put16(p + 8, 1);
    put16(p + 10, 24);
    put64(p + 12, UINT64_C(133750656001234567));
    put64(p + 20, UINT64_C(133750656002345678));
    put64(p + 28, UINT64_C(133750656003456789));
    put16(p + 36, 2);
    put16(p + 38, 0);
}

int main(void)
{
    unsigned char *data = calloc(180, 1);
    struct archive *a;
    struct archive_entry *entry;
    int r;

    if (data == NULL)
        return 2;
    memcpy(data, "PK\003\004", 4);
    put16(data + 4, 20);
    put16(data + 26, 1);
    put16(data + 28, 40);
    data[30] = 'a';
    extra(data + 31);

    memcpy(data + 71, "PK\001\002", 4);
    put16(data + 75, 20);
    put16(data + 77, 20);
    put16(data + 99, 1);
    put16(data + 101, 40);
    data[117] = 'a';
    extra(data + 118);

    memcpy(data + 158, "PK\005\006", 4);
    put16(data + 166, 1);
    put16(data + 168, 1);
    put32(data + 170, 87);
    put32(data + 174, 71);

    a = archive_read_new();
    if (a == NULL)
        return 2;
    archive_read_support_format_zip(a);
    r = archive_read_open_memory(a, data, 180);
    if (r == ARCHIVE_OK)
        r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_OK)
        printf("%lld %lld %lld\n",
            (long long)archive_entry_mtime(entry),
            (long long)archive_entry_atime(entry),
            (long long)archive_entry_birthtime(entry));
    archive_read_free(a);
    free(data);
    return r == ARCHIVE_OK ? 0 : 2;
}
