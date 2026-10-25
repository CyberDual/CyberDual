#include <stdio.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>

/*
 * Old GNU tar header with one sparse map entry whose offset field is
 * twelve octal digits and no trailing NUL.  That encoding is valid for
 * GNU numeric fields (the width is fixed; a terminator is optional).
 */
static unsigned char image[512 * 3];

static void
put_octal(unsigned char *p, unsigned width, unsigned long val)
{
	char tmp[32];
	int n;

	n = snprintf(tmp, sizeof(tmp), "%0*lo", (int)width - 1, val);
	if (n < 0 || (unsigned)n >= width)
		return;
	memcpy(p, tmp, n);
	p[n] = '\0';
}

static void
build_archive(void)
{
	unsigned char *h = image;
	unsigned sum;
	unsigned i;

	memset(image, 0, sizeof(image));
	memcpy(h + 0, "sparsefile", 10);
	put_octal(h + 100, 8, 0644);
	put_octal(h + 108, 8, 0);
	put_octal(h + 116, 8, 0);
	put_octal(h + 124, 12, 0);
	put_octal(h + 136, 12, 0);
	h[156] = 'S';
	memcpy(h + 257, "ustar  ", 8);
	memcpy(h + 265, "root", 4);
	memcpy(h + 297, "root", 4);
	/* 12 octal digits, no NUL: value 1. */
	memcpy(h + 386, "000000000001", 12);
	put_octal(h + 398, 12, 1);
	put_octal(h + 483, 12, 1);

	memset(h + 148, ' ', 8);
	sum = 0;
	for (i = 0; i < 512; i++)
		sum += h[i];
	snprintf((char *)h + 148, 8, "%06o", sum);
	h[154] = '\0';
	h[155] = ' ';
}

int
main(void)
{
	struct archive *a;
	struct archive_entry *e;
	int r;

	build_archive();
	a = archive_read_new();
	archive_read_support_format_tar(a);
	r = archive_read_open_memory(a, image, sizeof(image));
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open %d\n", r);
		return 1;
	}
	r = archive_read_next_header(a, &e);
	fprintf(stderr, "next_header=%d name=%s err=%s\n", r,
	    (r == ARCHIVE_OK) ? archive_entry_pathname(e) : "(none)",
	    archive_error_string(a) ? archive_error_string(a) : "");
	archive_read_free(a);
	return (r == ARCHIVE_OK) ? 0 : 2;
}
