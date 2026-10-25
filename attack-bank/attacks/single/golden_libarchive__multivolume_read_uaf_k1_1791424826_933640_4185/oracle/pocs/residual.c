#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return -1;
    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        return -1;
    }
    return fclose(f);
}

int main(void)
{
    char dir[] = "/tmp/libarchive-split-XXXXXX";
    char first[sizeof(dir) + 16], second[sizeof(dir) + 16];
    const char *names[3];
    unsigned char tar[1280] = {0};
    struct archive *a;
    struct archive_entry *entry;
    unsigned int sum = 0;
    size_t i;
    int result;

    if (mkdtemp(dir) == NULL)
        return 2;
    snprintf(first, sizeof(first), "%s/part1", dir);
    snprintf(second, sizeof(second), "%s/part2", dir);
    memcpy(tar, "input.txt", 9);
    memcpy(tar + 100, "0000644", 7);
    memcpy(tar + 108, "0000000", 7);
    memcpy(tar + 116, "0000000", 7);
    memcpy(tar + 124, "0000014", 7);
    memcpy(tar + 136, "00000000000", 11);
    memset(tar + 148, ' ', 8);
    tar[156] = '0';
    memcpy(tar + 257, "ustar\0", 6);
    memcpy(tar + 263, "00", 2);
    for (i = 0; i < 512; ++i)
        sum += tar[i];
    snprintf((char *)tar + 148, 8, "%06o", sum);
    tar[154] = '\0';
    tar[155] = ' ';
    memcpy(tar + 512, "hello world\n", 12);
    if (write_file(first, tar, 256) != 0 ||
        write_file(second, tar + 256, sizeof(tar) - 256) != 0)
        return 2;

    names[0] = first;
    names[1] = second;
    names[2] = NULL;
    a = archive_read_new();
    if (a == NULL)
        return 2;
    archive_read_support_filter_uu(a);
    archive_read_support_format_tar(a);
    result = archive_read_open_filenames(a, names, 4096);
    if (result == ARCHIVE_OK)
        result = archive_read_next_header(a, &entry);
    if (result != ARCHIVE_OK ||
        strcmp(archive_entry_pathname(entry), "input.txt") != 0)
        return 1;
    result = archive_read_next_header(a, &entry);
    archive_read_free(a);
    unlink(first);
    unlink(second);
    rmdir(dir);
    return result == ARCHIVE_RETRY || result == ARCHIVE_EOF ? 0 : 1;
}
