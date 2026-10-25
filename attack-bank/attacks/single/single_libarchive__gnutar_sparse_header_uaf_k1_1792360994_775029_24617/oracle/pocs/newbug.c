/*
 * Reads a tar archive holding an old-style GNU sparse entry whose
 * sparse map carries a base-256 encoded (negative) offset, through a
 * client read callback that hands out one 512-byte block per call.
 *
 * Build (against a libarchive tree built with -fsanitize=address):
 *   gcc -fsanitize=address -g newbug.c -I libarchive \
 *       build/libarchive/libarchive.a -lz -lbz2 -llzma -lexpat -o newbug
 * Run:
 *   ./newbug
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct reader_ctx {
	const unsigned char *data;
	size_t size;
	size_t pos;
	char *buf;
};

static ssize_t
my_read(struct archive *a, void *client_data, const void **buff)
{
	struct reader_ctx *ctx = (struct reader_ctx *)client_data;
	size_t chunk;

	(void)a;
	/* The previous buffer is no longer needed once we are
	 * called again: release it and hand out a fresh one. */
	free(ctx->buf);
	ctx->buf = NULL;

	if (ctx->pos >= ctx->size)
		return (0);
	chunk = ctx->size - ctx->pos;
	if (chunk > 512)
		chunk = 512;
	ctx->buf = (char *)malloc(chunk);
	if (ctx->buf == NULL)
		return (-1);
	memcpy(ctx->buf, ctx->data + ctx->pos, chunk);
	ctx->pos += chunk;
	*buff = ctx->buf;
	return ((ssize_t)chunk);
}

static int
my_close(struct archive *a, void *client_data)
{
	struct reader_ctx *ctx = (struct reader_ctx *)client_data;
	(void)a;
	free(ctx->buf);
	ctx->buf = NULL;
	return (0);
}

static void
octal(char *field, size_t len, long long v)
{
	char tmp[32];
	snprintf(tmp, sizeof(tmp), "%0*llo", (int)(len - 1),
	    (unsigned long long)v);
	memcpy(field, tmp, len - 1);
	field[len - 1] = '\0';
}

static void
fix_checksum(unsigned char *h)
{
	int i, sum = 0;
	memset(h + 148, ' ', 8);
	for (i = 0; i < 512; i++)
		sum += h[i];
	snprintf((char *)h + 148, 8, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';
}

int
main(void)
{
	static unsigned char arc[512 * 3];
	unsigned char *h = arc;
	struct reader_ctx ctx;
	struct archive *a;
	struct archive_entry *ae;
	int r, i;

	memset(arc, 0, sizeof(arc));

	/* --- Block 0: GNU tar header with old-style sparse entries --- */
	memcpy(h + 0, "sparsefile", 10);			/* name */
	octal((char *)h + 100, 8, 0644);			/* mode */
	octal((char *)h + 108, 8, 0);				/* uid */
	octal((char *)h + 116, 8, 0);				/* gid */
	octal((char *)h + 124, 12, 0);				/* size */
	octal((char *)h + 136, 12, 01234567);			/* mtime */
	h[156] = '0';						/* typeflag */
	memcpy(h + 257, "ustar  \0", 8);			/* GNU magic */
	memcpy(h + 265, "user", 4);				/* uname */
	memcpy(h + 297, "group", 5);				/* gname */
	/* sparse[0].offset: base-256 encoded negative value */
	for (i = 0; i < 12; i++)
		h[386 + i] = 0xff;
	/* sparse[0].numbytes: 1 */
	octal((char *)h + 398, 12, 1);
	fix_checksum(h);

	/* --- Blocks 1,2: end-of-archive null blocks --- */

	memset(&ctx, 0, sizeof(ctx));
	ctx.data = arc;
	ctx.size = sizeof(arc);

	a = archive_read_new();
	archive_read_support_format_tar(a);
	r = archive_read_open(a, &ctx, NULL, my_read, my_close);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		return (2);
	}
	r = archive_read_next_header(a, &ae);
	fprintf(stderr, "next_header -> %d (%s)\n", r,
	    r == ARCHIVE_OK ? "ok" : archive_error_string(a));
	archive_read_free(a);
	fprintf(stderr, "done\n");
	return (0);
}
