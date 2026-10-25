#include <archive.h>
#include <archive_entry.h>
#include <string.h>

/*
 * GNU ar member with an odd-sized filename table whose final byte is an
 * accepted pad, followed by a long-name reference into that table.
 */
static void
member_header(char *h, const char *name, const char *size)
{
	memset(h, ' ', 60);
	memcpy(h, name, strlen(name));
	h[16] = '0';
	h[28] = '0';
	h[34] = '0';
	memcpy(h + 40, "100644", 6);
	memcpy(h + 48, size, 10);
	h[58] = '`';
	h[59] = '\n';
}

int
main(void)
{
	char buf[256];
	size_t n;
	struct archive *a;
	struct archive_entry *entry;
	int r;

	memcpy(buf, "!<arch>\n", 8);
	n = 8;

	member_header(buf + n, "//", "3         ");
	n += 60;
	memcpy(buf + n, "ab\n", 3);
	n += 3;
	buf[n++] = '\n';

	member_header(buf + n, "/0", "0         ");
	n += 60;

	a = archive_read_new();
	if (a == NULL)
		return (1);
	archive_read_support_format_ar(a);
	r = archive_read_open_memory(a, buf, n);
	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		return (2);
	}
	r = archive_read_next_header(a, &entry);
	archive_read_free(a);
	return (r == ARCHIVE_OK ? 0 : 3);
}
