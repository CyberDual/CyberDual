#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

static const unsigned char zip_bytes[] = {
	0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,
	0x0c,0x00,0x74,0x65,0x73,0x74,0x2e,0x74,0x78,0x74,0x0a,0x00,0x08,0x00,
	0x00,0x00,0x00,0x00,0x01,0x00,0x18,0x00
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buf;
	int r;

	buf = malloc(sizeof(zip_bytes));
	if (buf == NULL)
		return 1;
	memcpy(buf, zip_bytes, sizeof(zip_bytes));

	a = archive_read_new();
	if (a == NULL)
		return 1;
	archive_read_support_format_zip(a);
	r = archive_read_open_memory(a, buf, sizeof(zip_bytes));
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		return 1;
	}
	r = archive_read_next_header(a, &ae);
	fprintf(stderr, "header %d\n", r);
	archive_read_free(a);
	free(buf);
	return r == ARCHIVE_OK ? 0 : 2;
}
