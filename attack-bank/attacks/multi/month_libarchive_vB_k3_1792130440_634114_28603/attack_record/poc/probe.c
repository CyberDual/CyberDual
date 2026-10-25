#include <archive.h>
#include <archive_entry.h>
#include <string.h>

/*
 * GNU ar member: a filename table followed by one member whose long
 * name is an offset into that table. Field widths match the ar header.
 */
static void
field(char *dst, size_t width, const char *text)
{
	size_t n = strlen(text);

	if (n > width)
		n = width;
	memcpy(dst, text, n);
	if (n < width)
		memset(dst + n, ' ', width - n);
}

static size_t
build_archive(unsigned char *buf)
{
	unsigned char *p = buf;

	memcpy(p, "!<arch>\n", 8);
	p += 8;

	/* Even-sized filename table: "foo/\n" plus the usual pad newline. */
	field((char *)p + 0, 16, "//");
	field((char *)p + 16, 12, "");
	field((char *)p + 28, 6, "");
	field((char *)p + 34, 6, "");
	field((char *)p + 40, 8, "");
	field((char *)p + 48, 10, "6");
	memcpy(p + 58, "`\n", 2);
	p += 60;
	memcpy(p, "foo/\n\n", 6);
	p += 6;

	/* Offset equal to the on-disk table length, one data byte. */
	field((char *)p + 0, 16, "/6");
	field((char *)p + 16, 12, "0");
	field((char *)p + 28, 6, "0");
	field((char *)p + 34, 6, "0");
	field((char *)p + 40, 8, "100644");
	field((char *)p + 48, 10, "1");
	memcpy(p + 58, "`\n", 2);
	p += 60;
	*p++ = 'x';
	*p++ = '\n';

	return (size_t)(p - buf);
}

int
main(void)
{
	unsigned char buf[8 + 60 + 6 + 60 + 2];
	size_t len;
	struct archive *a;
	struct archive_entry *entry;

	len = build_archive(buf);
	a = archive_read_new();
	if (a == NULL)
		return 0;
	archive_read_support_format_ar(a);
	if (archive_read_open_memory(a, buf, len) == ARCHIVE_OK)
		archive_read_next_header(a, &entry);
	archive_read_free(a);
	return 0;
}
