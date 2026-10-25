/*
 * Self-contained probe: feeds a crafted uuencoded stream to libarchive's
 * uu read filter entirely through the public API.
 *
 * The stream is a well-formed-looking uuencoded file whose first data line
 * is a normal full-length line (so the bidder accepts it), followed by many
 * one-character lines.  Each one-character line carries a length byte ('M'
 * == 45 decoded bytes) but no payload characters, which a robust decoder
 * must reject as truncated.
 *
 * Built against the unmodified library this simply fails to decode and the
 * read returns an error.  No internal symbols are used.
 */
#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	static const char head[] = "begin 644 data\n";
	/* First data line: length byte 'M' (45) plus 60 payload chars, a
	 * perfectly ordinary full-length uu line. */
	static const char first_line[] =
	    "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n";
	/* Number of truncated one-character "M" lines that follow. */
	const int short_lines = 3000;
	/* Trailing pad so lookahead stays inside the buffer. */
	static const char tail[] =
	    "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n"
	    " \n"
	    "end\n";

	size_t cap = sizeof(head) + sizeof(first_line) +
	    (size_t)short_lines * 2 + sizeof(tail) + 16;
	char *buf = malloc(cap);
	size_t off = 0;
	int i;

	if (buf == NULL) {
		fprintf(stderr, "oom\n");
		return (2);
	}

	memcpy(buf + off, head, sizeof(head) - 1);
	off += sizeof(head) - 1;
	memcpy(buf + off, first_line, sizeof(first_line) - 1);
	off += sizeof(first_line) - 1;
	for (i = 0; i < short_lines; i++) {
		buf[off++] = 'M';
		buf[off++] = '\n';
	}
	memcpy(buf + off, tail, sizeof(tail) - 1);
	off += sizeof(tail) - 1;

	{
		struct archive *a = archive_read_new();
		struct archive_entry *ae;
		char data[8192];
		ssize_t n;
		int r;

		if (a == NULL) {
			fprintf(stderr, "archive_read_new failed\n");
			return (2);
		}
		archive_read_support_filter_uu(a);
		archive_read_support_format_tar(a);

		r = archive_read_open_memory(a, buf, off);
		if (r != ARCHIVE_OK) {
			fprintf(stderr, "open: %s\n", archive_error_string(a));
			archive_read_free(a);
			free(buf);
			return (1);
		}

		r = archive_read_next_header(a, &ae);
		if (r == ARCHIVE_OK) {
			while ((n = archive_read_data(a, data,
			    sizeof(data))) > 0)
				;
		}
		printf("next_header=%d (%s)\n", r,
		    archive_error_string(a) ? archive_error_string(a) : "ok");

		archive_read_free(a);
	}

	free(buf);
	return (0);
}
