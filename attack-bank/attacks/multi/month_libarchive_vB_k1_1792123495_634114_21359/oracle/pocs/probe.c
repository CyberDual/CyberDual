#include <archive.h>
#include <archive_entry.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	static const char head[] = "begin 644 name\n!@$\n";
	static const char tail[] = "\nend\n";
	enum { LINE = 32767, PAY = 62 };
	struct archive *a;
	struct archive_entry *e;
	unsigned char *buf;
	size_t n = 0;
	size_t i;
	int r;

	buf = malloc(sizeof(head) - 1 + LINE + sizeof(tail));
	if (buf == NULL)
		return 0;
	memcpy(buf + n, head, sizeof(head) - 1);
	n += sizeof(head) - 1;
	buf[n++] = 'N';
	for (i = 0; i < PAY; i++)
		buf[n++] = 'A';
	for (i = 1 + PAY; i < LINE; i++)
		buf[n++] = ' ';
	memcpy(buf + n, tail, sizeof(tail) - 1);
	n += sizeof(tail) - 1;

	a = archive_read_new();
	if (a == NULL) {
		free(buf);
		return 0;
	}
	archive_read_support_filter_uu(a);
	archive_read_support_format_raw(a);
	r = archive_read_open_memory(a, buf, n);
	if (r == ARCHIVE_OK)
		r = archive_read_next_header(a, &e);
	if (r == ARCHIVE_OK) {
		char tmp[64];
		archive_read_data(a, tmp, sizeof(tmp));
	}
	archive_read_free(a);
	free(buf);
	return 0;
}
