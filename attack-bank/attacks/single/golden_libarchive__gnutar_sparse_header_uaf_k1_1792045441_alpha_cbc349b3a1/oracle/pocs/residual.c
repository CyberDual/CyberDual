/*
 * Read an old-style GNU sparse tar through archive_read_open().
 * Each callback returns a fresh 512-byte block and releases the previous
 * one, which the read API allows.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct reader {
	const unsigned char *data;
	size_t size;
	size_t off;
	unsigned char *buf;
};

static la_ssize_t
read_cb(struct archive *a, void *client, const void **buff)
{
	struct reader *r = client;
	size_t n;

	(void)a;
	free(r->buf);
	r->buf = NULL;
	if (r->off >= r->size) {
		*buff = NULL;
		return (0);
	}
	n = 512;
	if (r->size - r->off < n)
		n = r->size - r->off;
	r->buf = malloc(n);
	if (r->buf == NULL)
		return (-1);
	memcpy(r->buf, r->data + r->off, n);
	r->off += n;
	*buff = r->buf;
	return ((la_ssize_t)n);
}

static void
put_octal(char *dst, size_t n, unsigned long long v)
{
	char tmp[32];
	int i;

	memset(dst, '0', n);
	snprintf(tmp, sizeof(tmp), "%0*llo", (int)(n - 1), v);
	/* Keep a trailing NUL inside the field. */
	memcpy(dst, tmp, n - 1);
	dst[n - 1] = '\0';
}

static void
build_archive(unsigned char *out, size_t cap)
{
	unsigned char *h = out;
	unsigned sum;
	size_t i;

	memset(out, 0, cap);
	memcpy(h + 0, "sparse", 6);
	put_octal((char *)h + 100, 8, 0644);
	put_octal((char *)h + 108, 8, 123);
	put_octal((char *)h + 116, 8, 456);
	put_octal((char *)h + 124, 12, 0);
	put_octal((char *)h + 136, 12, 123456789ULL);
	h[156] = 'S';
	memcpy(h + 257, "ustar  ", 7); /* magic, final byte already NUL */
	memcpy(h + 265, "user", 4);
	memcpy(h + 297, "group", 5);
	/* One sparse extent so the old GNU map is parsed, and an extension. */
	put_octal((char *)h + 386, 12, 0);
	put_octal((char *)h + 398, 12, 0);
	h[482] = '1';
	put_octal((char *)h + 483, 12, 0);

	memset(h + 148, ' ', 8);
	sum = 0;
	for (i = 0; i < 512; i++)
		sum += h[i];
	snprintf((char *)h + 148, 8, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';
}

int
main(void)
{
	unsigned char image[512 * 4];
	struct reader r;
	struct archive *a;
	struct archive_entry *ae;
	int rc;

	build_archive(image, sizeof(image));
	memset(&r, 0, sizeof(r));
	r.data = image;
	r.size = sizeof(image);

	a = archive_read_new();
	if (a == NULL)
		return (1);
	archive_read_support_format_tar(a);
	if (archive_read_open(a, &r, NULL, read_cb, NULL) != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		return (1);
	}
	rc = archive_read_next_header(a, &ae);
	if (rc != ARCHIVE_OK) {
		fprintf(stderr, "header: %s (%d)\n", archive_error_string(a), rc);
		archive_read_free(a);
		free(r.buf);
		return (1);
	}
	printf("name=%s mode=%o uid=%ld gid=%ld mtime=%ld\n",
	    archive_entry_pathname(ae),
	    (unsigned)archive_entry_mode(ae) & 0777,
	    (long)archive_entry_uid(ae),
	    (long)archive_entry_gid(ae),
	    (long)archive_entry_mtime(ae));
	rc = archive_read_next_header(a, &ae);
	printf("next=%d\n", rc);
	archive_read_free(a);
	free(r.buf);
	return (0);
}
