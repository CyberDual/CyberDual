/*
 * Exercises a two-volume iso9660 archive whose only regular file
 * stores its data at a lower extent than the directory referencing
 * it, so the reader has to seek backwards across the volume boundary
 * while walking the entries.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

#define	LB		2048
#define	TOTAL_LBA	24
#define	SPLIT_LBA	18

static void
put16le(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
}

static void
put16be(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v >> 8);
	p[1] = (unsigned char)v;
}

static void
put32le(unsigned char *p, unsigned v)
{
	put16le(p, v);
	put16le(p + 2, v >> 16);
}

static void
put32be(unsigned char *p, unsigned v)
{
	put16be(p, v >> 16);
	put16be(p + 2, v);
}

static size_t
dir_record(unsigned char *p, const char *name, size_t namelen,
    unsigned extent, unsigned size, unsigned flags)
{
	size_t len = 33 + namelen;

	if (len % 2)
		len++;
	memset(p, 0, len);
	p[0] = (unsigned char)len;
	put32le(p + 2, extent);
	put32be(p + 6, extent);
	put32le(p + 10, size);
	put32be(p + 14, size);
	p[25] = (unsigned char)flags;
	put16le(p + 28, 1);
	put16be(p + 30, 1);
	p[32] = (unsigned char)namelen;
	memcpy(p + 33, name, namelen);
	return (len);
}

static int
write_file(const char *path, const unsigned char *data, size_t len)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL)
		return (-1);
	if (fwrite(data, 1, len, f) != len) {
		fclose(f);
		return (-1);
	}
	fclose(f);
	return (0);
}

int
main(void)
{
	unsigned char *img;
	unsigned char *p;
	const char *vols[3];
	struct archive *a;
	struct archive_entry *ae;
	char buf[256];
	size_t off;
	int r;

	img = calloc(TOTAL_LBA, LB);
	if (img == NULL)
		return (1);

	/* Primary volume descriptor at LBA 16. */
	p = img + 16 * LB;
	p[0] = 1;
	memcpy(p + 1, "CD001", 5);
	p[6] = 1;
	memset(p + 8, ' ', 64);
	put32le(p + 80, TOTAL_LBA);
	put32be(p + 84, TOTAL_LBA);
	put16le(p + 120, 1);
	put16be(p + 122, 1);
	put16le(p + 124, 1);
	put16be(p + 126, 1);
	put16le(p + 128, LB);
	put16be(p + 130, LB);
	put32le(p + 132, 10);
	put32be(p + 136, 10);
	put32le(p + 140, 18);
	dir_record(p + 156, "\0", 1, 22, 2048, 0x02);
	p[881] = 1;

	/* Volume descriptor set terminator at LBA 17. */
	p = img + 17 * LB;
	p[0] = 255;
	memcpy(p + 1, "CD001", 5);
	p[6] = 1;

	/* File data at LBA 20, before the directory referencing it. */
	memset(img + 20 * LB, 'A', LB);

	/* Root directory at LBA 22. */
	p = img + 22 * LB;
	off = 0;
	off += dir_record(p + off, "\0", 1, 22, 2048, 0x02);
	off += dir_record(p + off, "\1", 1, 22, 2048, 0x02);
	off += dir_record(p + off, "A", 1, 20, 2048, 0x00);

	if (write_file("/tmp/iso_mv_vol0", img, SPLIT_LBA * LB) != 0 ||
	    write_file("/tmp/iso_mv_vol1", img + SPLIT_LBA * LB,
		(TOTAL_LBA - SPLIT_LBA) * LB) != 0) {
		free(img);
		return (1);
	}
	free(img);

	vols[0] = "/tmp/iso_mv_vol0";
	vols[1] = "/tmp/iso_mv_vol1";
	vols[2] = NULL;

	a = archive_read_new();
	archive_read_support_format_iso9660(a);
	if (archive_read_open_filenames(a, vols, 4096) != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		archive_read_free(a);
		return (1);
	}
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		fprintf(stderr, "entry: %s\n", archive_entry_pathname(ae));
		archive_read_data(a, buf, sizeof(buf));
	}
	fprintf(stderr, "done: %s\n", archive_error_string(a));
	archive_read_free(a);
	return (0);
}
