#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const size_t length = 65537;
    const size_t capacity = 1024 * 1024;
    unsigned char *archive_data = malloc(capacity);
    unsigned char *file_data = malloc(length);
    struct archive *writer = archive_write_new();
    struct archive *reader;
    struct archive_entry *entry = archive_entry_new();
    char output[4096];
    size_t archive_size = 0;
    int result;

    if (!archive_data || !file_data || !writer || !entry) return 2;
    memset(file_data, 'A', length);
    if (archive_write_set_format_7zip(writer) != ARCHIVE_OK ||
        archive_write_set_format_option(writer, "7zip", "compression", "ppmd") != ARCHIVE_OK ||
        archive_write_open_memory(writer, archive_data, capacity, &archive_size) != ARCHIVE_OK)
        return 2;
    archive_entry_set_pathname(entry, "sample.txt");
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, (la_int64_t)length);
    if (archive_write_header(writer, entry) != ARCHIVE_OK ||
        archive_write_data(writer, file_data, length) != (la_ssize_t)length ||
        archive_write_close(writer) != ARCHIVE_OK)
        return 2;
    archive_entry_free(entry);
    archive_write_free(writer);

    reader = archive_read_new();
    archive_read_support_format_7zip(reader);
    result = archive_read_open_memory(reader, archive_data, archive_size);
    if (result == ARCHIVE_OK)
        result = archive_read_next_header(reader, &entry);
    if (result == ARCHIVE_OK)
        while ((result = (int)archive_read_data(reader, output, sizeof(output))) > 0)
            ;
    if (result < 0)
        fprintf(stderr, "%s\n", archive_error_string(reader));
    archive_read_free(reader);
    free(file_data);
    free(archive_data);
    return result < 0 ? 1 : 0;
}
