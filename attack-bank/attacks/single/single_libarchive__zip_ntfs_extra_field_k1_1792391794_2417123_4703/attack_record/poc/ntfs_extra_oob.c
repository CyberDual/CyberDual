/*
 * Reads a ZIP archive whose local file header carries an NTFS (0x000a)
 * extra field in which the file-times attribute claims more data than
 * the extra field actually holds.  Uses only the public libarchive API.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A minimal ZIP local file header ("a", stored, empty) whose 32-byte
 * extra block is a single NTFS extra field.  Its times attribute
 * declares a size of 24 bytes while only 20 bytes are present before
 * the end of the archive. */
static const unsigned char zipdata[] = {
	0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x61, 0x0a,
	0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x18, 0x00, 0x00, 0xc0, 0x89, 0x76, 0x45,
	0x3c, 0xda, 0x01, 0x80, 0x77, 0x9e, 0x21, 0x4e,
	0x3c, 0xda, 0x01, 0x00, 0x00, 0x00, 0x00
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *entry;
	unsigned char *buf;
	int r;

	/* Heap-allocate the exact archive size so that any over-read is
	 * caught by the allocator/ASan red zone. */
	buf = malloc(sizeof(zipdata));
	if (buf == NULL) {
		fprintf(stderr, "malloc failed\n");
		return (1);
	}
	memcpy(buf, zipdata, sizeof(zipdata));

	a = archive_read_new();
	archive_read_support_format_zip(a);
	r = archive_read_open_memory(a, buf, sizeof(zipdata));
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		archive_read_free(a);
		free(buf);
		return (1);
	}

	r = archive_read_next_header(a, &entry);
	if (r >= ARCHIVE_WARN) {
		printf("entry: %s\n", archive_entry_pathname(entry));
		printf("mtime: %ld\n", (long)archive_entry_mtime(entry));
		printf("atime: %ld\n", (long)archive_entry_atime(entry));
		if (archive_entry_birthtime_is_set(entry))
			printf("birthtime: %ld\n",
			    (long)archive_entry_birthtime(entry));
	} else if (r == ARCHIVE_EOF) {
		printf("EOF\n");
	} else {
		fprintf(stderr, "read header failed: %s\n",
		    archive_error_string(a));
	}

	archive_read_free(a);
	free(buf);
	return (0);
}
