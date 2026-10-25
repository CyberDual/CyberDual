// PoC: lzma_index_dup() on an Index whose Stream holds more than
// INDEX_GROUP_SIZE (512) Records corrupts the heap inside
// index_dup_stream() and aborts under AddressSanitizer.
//
// Build against liblzma, e.g.:
//   cc -fsanitize=address -g -o dup_many_records dup_many_records.c -llzma
//
// Exits 0 on a correct library; dies with a heap-buffer-overflow
// report in lzma_index_dup() on a vulnerable one.

#include <stdio.h>
#include <lzma.h>

int main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		return 2;
	}

	// A single Stream with 1023 Blocks, so that its Records span
	// two internal allocation groups (INDEX_GROUP_SIZE is 512).
	for (int i = 0; i < 1023; i++) {
		if (lzma_index_append(idx, NULL, 8, 4) != LZMA_OK) {
			fprintf(stderr, "lzma_index_append failed\n");
			return 2;
		}
	}

	// Duplicate the Index.
	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL) {
		fprintf(stderr, "lzma_index_dup failed\n");
		return 2;
	}

	printf("checks: %08x\n", lzma_index_checks(copy));
	printf("blocks: %llu\n",
			(unsigned long long)lzma_index_block_count(copy));

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);
	return 0;
}
