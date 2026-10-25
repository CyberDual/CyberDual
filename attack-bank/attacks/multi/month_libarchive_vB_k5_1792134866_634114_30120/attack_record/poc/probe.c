#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>

/*
 * GNU ar member header is 60 bytes. The filename table ("//") is an
 * in-format special member: an odd payload is followed by one pad byte
 * so the next member stays on an even boundary. The payload here is a
 * single long-name slot whose last byte is the GNU name introducer.
 */
static void
put_field(char *dst, size_t n, const char *s)
{
	size_t i, len;

	len = strlen(s);
	if (len > n)
		len = n;
	memcpy(dst, s, len);
	for (i = len; i < n; i++)
		dst[i] = ' ';
}

int
main(void)
{
	char image[8 + 60 + 8];
	char *p;
	struct archive *a;
	struct archive_entry *entry;
	size_t len;
	int r;

	memset(image, 0, sizeof(image));
	memcpy(image, "!<arch>\n", 8);
	p = image + 8;
	put_field(p + 0, 16, "//");
	put_field(p + 16, 12, "0");
	put_field(p + 28, 6, "0");
	put_field(p + 34, 6, "0");
	put_field(p + 40, 8, "0");
	put_field(p + 48, 10, "5");
	p[58] = '`';
	p[59] = '\n';
	/* Payload "xxxx/" (odd) plus the required even-byte pad. */
	memcpy(p + 60, "xxxx/\n", 6);
	len = 8 + 60 + 6;

	a = archive_read_new();
	if (a == NULL)
		return (1);
	if (archive_read_support_format_ar(a) != ARCHIVE_OK)
		return (1);
	if (archive_read_open_memory(a, image, len) != ARCHIVE_OK)
		return (1);

	/* Parsing the filename-table member happens inside next_header. */
	r = archive_read_next_header(a, &entry);
	(void)r;
	(void)entry;

	archive_read_free(a);
	return (0);
}
