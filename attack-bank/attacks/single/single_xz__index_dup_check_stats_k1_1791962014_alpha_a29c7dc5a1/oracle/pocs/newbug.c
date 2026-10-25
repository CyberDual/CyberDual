// SPDX-License-Identifier: 0BSD

#include "lzma.h"

#include <stdint.h>


int
main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	// Enough Records to make the Stream use multiple Record groups.
	for (uint32_t n = 0; n < 600; ++n) {
		if (lzma_index_append(idx, NULL, 16, 16) != LZMA_OK) {
			lzma_index_end(idx, NULL);
			return 1;
		}
	}

	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL) {
		lzma_index_end(idx, NULL);
		return 1;
	}

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);
	return 0;
}
