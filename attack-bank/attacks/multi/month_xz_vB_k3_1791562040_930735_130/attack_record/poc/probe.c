#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

static lzma_index *
make_one_stream(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		exit(1);
	}

	if (lzma_index_append(idx, NULL, 8, 8) != LZMA_OK) {
		fprintf(stderr, "lzma_index_append failed\n");
		exit(1);
	}

	return idx;
}

int
main(void)
{
	lzma_index *acc = make_one_stream();

	for (int i = 0; i < 3; i++) {
		lzma_index *next = make_one_stream();

		if (lzma_index_cat(next, acc, NULL) != LZMA_OK) {
			fprintf(stderr, "lzma_index_cat failed\n");
			exit(1);
		}

		acc = next;
	}

	lzma_index_end(acc, NULL);

	printf("done\n");
	return 0;
}
