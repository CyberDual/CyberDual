#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>

static void
hdr(char *d, const char *name, int size)
{
	char tmp[16];

	memset(d, ' ', 60);
	memcpy(d, name, strlen(name));
	memcpy(d + 16, "0           ", 12);
	memcpy(d + 28, "0     ", 6);
	memcpy(d + 34, "0     ", 6);
	memcpy(d + 40, "644     ", 8);
	snprintf(tmp, sizeof(tmp), "%-10d", size);
	memcpy(d + 48, tmp, 10);
	d[58] = '`';
	d[59] = '\n';
}

int
main(void)
{
	unsigned char table[8] = {
		'a', '\\', 0, 'T', 'A', 'I', 'L', '\n'
	};
	char buf[256];
	char *p;
	struct archive *a;
	struct archive_entry *e;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, "!<arch>\n", 8);
	p = buf + 8;
	hdr(p, "//", 8);
	p += 60;
	memcpy(p, table, sizeof(table));
	p += sizeof(table);
	hdr(p, "/2", 0);
	p += 60;

	a = archive_read_new();
	if (a == NULL)
		return (0);
	archive_read_support_format_ar(a);
	if (archive_read_open_memory(a, buf, (size_t)(p - buf)) != ARCHIVE_OK) {
		archive_read_free(a);
		return (0);
	}
	(void)archive_read_next_header(a, &e);
	archive_read_free(a);
	return (0);
}
