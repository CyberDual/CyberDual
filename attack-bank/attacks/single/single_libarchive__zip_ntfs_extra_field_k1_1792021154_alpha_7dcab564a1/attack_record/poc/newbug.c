/*
 * Reproducer: a ZIP local file header whose NTFS (0x000a) extra field
 * runs to the very end of the archive.  The extra field declares a
 * "last modified time" attribute of size 8, but only 4 bytes of the
 * attribute data are actually present.
 *
 * Run against an AddressSanitizer build of libarchive:
 *   - a reader that does not parse the NTFS field at all exits cleanly;
 *   - a reader that trusts the declared attribute size without
 *     bounding it against the remaining extra-field length reads past
 *     the end of the buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

int
main(void)
{
	/* A single streaming ZIP entry, truncated right after the
	 * extra field of its local file header. */
	static const unsigned char zip_data[] = {
		0x50, 0x4b, 0x03, 0x04, /* Local file header signature */
		0x14, 0x00,             /* Version needed to extract */
		0x08, 0x00,             /* Flags: streaming entry */
		0x00, 0x00,             /* Compression: stored */
		0x21, 0x50, 0x24, 0x36, /* DOS modification time and date */
		0x00, 0x00, 0x00, 0x00, /* CRC-32 (in the data descriptor) */
		0x00, 0x00, 0x00, 0x00, /* Compressed size (data descriptor) */
		0x00, 0x00, 0x00, 0x00, /* Uncompressed size (data descr.) */
		0x01, 0x00,             /* File name length */
		0x10, 0x00,             /* Extra field length: 16 */
		'a',                    /* File name */
		/* NTFS extra field */
		0x0a, 0x00,             /* Header id 0x000a */
		0x0c, 0x00,             /* Data size: 12 */
		0x00, 0x00, 0x00, 0x00, /* Reserved */
		0x01, 0x00,             /* Attribute tag: mtime */
		0x08, 0x00,             /* Attribute size: 8 */
		0x78, 0x56, 0x34, 0x12, /* Only 4 of the 8 bytes are present */
	};
	unsigned char *buf;
	struct archive *a;
	struct archive_entry *entry;
	int r;

	buf = malloc(sizeof(zip_data));
	if (buf == NULL)
		return (1);
	memcpy(buf, zip_data, sizeof(zip_data));

	a = archive_read_new();
	if (a == NULL) {
		free(buf);
		return (1);
	}
	archive_read_support_format_zip(a);
	if (archive_read_open_memory(a, buf, sizeof(zip_data)) != ARCHIVE_OK) {
		archive_read_free(a);
		free(buf);
		return (1);
	}

	while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK
	    || r == ARCHIVE_WARN) {
		printf("entry: %s mtime=%lld atime=%lld birthtime=%lld\n",
		    archive_entry_pathname(entry),
		    (long long)archive_entry_mtime(entry),
		    (long long)archive_entry_atime(entry),
		    (long long)archive_entry_birthtime(entry));
	}

	archive_read_free(a);
	free(buf);
	return (0);
}
