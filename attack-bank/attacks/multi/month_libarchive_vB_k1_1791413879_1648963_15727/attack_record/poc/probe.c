#include <archive.h>
#include <archive_entry.h>
#include <string.h>

int main(void)
{
    unsigned char input[512];
    const char *head = "WARC/1.0\r\n"
        "WARC-Type: resource\r\n"
        "WARC-Date: 2014-06-10T10:10:10Z\r\n"
        "WARC-Target-URI: file://x";
    const char *tail = "\r\nContent-Length: 0\r\n\r\n\r\n\r\n";
    struct archive *a;
    struct archive_entry *entry;
    size_t n = strlen(head);

    memcpy(input, head, n);
    memset(input + n, 0, 96);
    n += 96;
    memcpy(input + n, tail, strlen(tail));
    n += strlen(tail);

    a = archive_read_new();
    if (a == NULL)
        return 2;
    archive_read_support_filter_none(a);
    archive_read_support_format_warc(a);
    if (archive_read_open_memory(a, input, n) != ARCHIVE_OK)
        return 3;
    archive_read_next_header(a, &entry);
    archive_read_free(a);
    return 0;
}
