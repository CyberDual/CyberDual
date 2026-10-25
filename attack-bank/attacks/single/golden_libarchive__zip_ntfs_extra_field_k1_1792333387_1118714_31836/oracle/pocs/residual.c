/*
 * Reads a small in-memory ZIP whose local-header extra data carries an
 * NTFS (0x000A) extra field. The field declares a Tag 1 (timestamps)
 * attribute block of size 24, but the field (and the archive) ends
 * before the block's FILETIME data is actually present.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char zip_bytes[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x00, 0x00, 0x00, 0x00, /* CRC-32 = 0 */
	0x00, 0x00, 0x00, 0x00, /* compressed size = 0 */
	0x00, 0x00, 0x00, 0x00, /* uncompressed size = 0 */
	0x01, 0x00,             /* file name length = 1 */
	0x12, 0x00,             /* extra field length = 18 */
	0x61,                   /* "a" */
/* extra field 0x000a "NTFS", declared size 14, present to end of archive */
	0x0a, 0x00,             /* extra field ID = 0x000a */
	0x0e, 0x00,             /* extra field size = 14 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,             /* NTFS attribute tag 1 (timestamps) */
	0x18, 0x00,             /* NTFS attribute 1 size = 24 */
	0x40, 0x42, 0x7c, 0xc6, 0x47, 0x17, /* only 6 FILETIME bytes follow */
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	void *buff;
	int r;

	buff = malloc(sizeof(zip_bytes));
	if (buff == NULL) {
		fprintf(stderr, "malloc failed\n");
		return 1;
	}
	memcpy(buff, zip_bytes, sizeof(zip_bytes));

	a = archive_read_new();
	archive_read_support_format_zip(a);
	r = archive_read_open_memory(a, buff, sizeof(zip_bytes));
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		return 1;
	}
	r = archive_read_next_header(a, &ae);
	if (r == ARCHIVE_OK) {
		printf("entry: %s mtime=%lld\n", archive_entry_pathname(ae),
		    (long long)archive_entry_mtime(ae));
		for (;;) {
			char data[64];
			if (archive_read_data(a, data, sizeof(data)) <= 0)
				break;
		}
		r = archive_read_next_header(a, &ae);
	}
	printf("done r=%d (%s)\n", r,
	    r == ARCHIVE_EOF ? "EOF" : archive_error_string(a));
	archive_read_free(a);
	free(buff);
	return 0;
}
