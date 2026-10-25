/*
 * Reads an archive through the wide-character filename API
 * (archive_read_open_filename_w) using a non-ASCII path.
 */
#include <archive.h>
#include <archive_entry.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void
make_tar(const char *path)
{
	unsigned char blk[512];
	unsigned int sum;
	size_t i;
	FILE *f;

	memset(blk, 0, sizeof(blk));
	snprintf((char *)blk + 0, 100, "data.bin");
	memcpy(blk + 100, "0000644", 8);
	memcpy(blk + 108, "0000000", 8);
	memcpy(blk + 116, "0000000", 8);
	snprintf((char *)blk + 124, 12, "%011lo", 0UL);
	snprintf((char *)blk + 136, 12, "%011lo", 1600000000UL);
	memset(blk + 148, ' ', 8);
	blk[156] = '0';
	memcpy(blk + 257, "ustar", 6);
	memcpy(blk + 263, "00", 3);
	sum = 0;
	for (i = 0; i < sizeof(blk); i++)
		sum += blk[i];
	snprintf((char *)blk + 148, 8, "%06o", sum);

	f = fopen(path, "wb");
	if (f == NULL)
		return;
	fwrite(blk, 1, sizeof(blk), f);
	fclose(f);
}

int
main(void)
{
	char mbs[256];
	wchar_t wname[256];
	struct archive *a;
	struct archive_entry *ae;
	size_t i, n;
	int r;

	setlocale(LC_ALL, "C.UTF-8");

	/* Archive path containing non-ASCII characters. */
	n = snprintf(mbs, sizeof(mbs), "/tmp/newbug_wcs.tar.");
	for (i = 0; i < 16; i++)
		n += snprintf(mbs + n, sizeof(mbs) - n, "\xC3\xA9"); /* é */
	make_tar(mbs);

	n = mbstowcs(wname, mbs, 200);
	wname[n] = L'\0';

	a = archive_read_new();
	archive_read_support_format_tar(a);
	r = archive_read_open_filename_w(a, wname, 10240);
	if (r == ARCHIVE_OK) {
		while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK)
			;
	}
	archive_read_free(a);
	return 0;
}
