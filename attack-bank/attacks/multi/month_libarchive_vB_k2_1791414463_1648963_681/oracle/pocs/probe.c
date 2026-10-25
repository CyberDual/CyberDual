#include <archive.h>
#include <archive_entry.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
    static const char warc[] =
        "WARC/1.0\r\n"
        "WARC-Type: resource\r\n"
        "WARC-Date: 2014-06-10T10:10:10Z\r\n"
        "WARC-Target-URI: file://\0"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "\r\n\r\n";
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    int status;
    if (a == NULL)
        return 2;
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    status = archive_read_open_memory(a, warc, sizeof(warc) - 1);
    if (status == ARCHIVE_OK)
        status = archive_read_next_header(a, &entry);
    archive_read_free(a);
    return status == ARCHIVE_FATAL ? 1 : 0;
}
