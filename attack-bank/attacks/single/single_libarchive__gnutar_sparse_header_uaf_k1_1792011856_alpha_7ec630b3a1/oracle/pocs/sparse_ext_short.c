/*
 * Read a tar archive that ends in the middle of an old-style GNU
 * sparse extension record.  The entry declares a sparse map that
 * continues past the end of the archive.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_SIZE (512 + 504)

static void
set_field(char *p, size_t n, unsigned long value)
{
	snprintf(p, n, "%0*lo", (int)(n - 1), value);
}

static void
finish_checksum(unsigned char *h)
{
	unsigned int sum = 0;
	size_t i;

	for (i = 0; i < 512; i++) {
		if (i >= 148 && i < 156)
			sum += ' ';
		else
			sum += h[i];
	}
	snprintf((char *)h + 148, 8, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';
}

static void
build_archive(unsigned char *p)
{
	memset(p, 0, ARCHIVE_SIZE);

	memcpy(p, "s", 1);				/* name */
	set_field((char *)p + 100, 8, 0644);		/* mode */
	set_field((char *)p + 108, 8, 0);		/* uid */
	set_field((char *)p + 116, 8, 0);		/* gid */
	set_field((char *)p + 124, 12, 0);		/* size */
	set_field((char *)p + 136, 12, 0);		/* mtime */
	p[156] = 'S';					/* GNU sparse file */
	memcpy(p + 257, "ustar  \0", 8);		/* GNU magic */
	set_field((char *)p + 386, 12, 1);		/* sparse[0].offset */
	set_field((char *)p + 398, 12, 1);		/* sparse[0].numbytes */
	p[482] = 1;					/* isextended */
	set_field((char *)p + 483, 12, 2);		/* realsize */

	finish_checksum(p);

	/* The continuation record is cut short by eight bytes. */
	memset(p + 512, 0, 504);
}

int
main(void)
{
	struct archive *a;
	struct archive_entry *entry;
	unsigned char *data;
	int r;

	data = malloc(ARCHIVE_SIZE);
	if (data == NULL) {
		fprintf(stderr, "out of memory\n");
		return (1);
	}
	build_archive(data);

	a = archive_read_new();
	if (a == NULL) {
		fprintf(stderr, "archive_read_new failed\n");
		free(data);
		return (1);
	}
	if (archive_read_support_format_tar(a) != ARCHIVE_OK) {
		fprintf(stderr, "archive_read_support_format_tar failed\n");
		archive_read_free(a);
		free(data);
		return (1);
	}
	r = archive_read_open_memory(a, data, ARCHIVE_SIZE);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "archive_read_open_memory failed\n");
		archive_read_free(a);
		free(data);
		return (1);
	}

	r = archive_read_next_header(a, &entry);
	printf("archive_read_next_header returned %d\n", r);

	archive_read_free(a);
	free(data);
	return (0);
}
