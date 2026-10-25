// SPDX-License-Identifier: 0BSD
//
// PoC: duplicate a multi-Stream lzma_index whose Streams have known
// Stream Flags, then query the Check bitmask. Exercises only the public
// liblzma API with valid (in-contract) inputs.
//
//   cc -fsanitize=address -I<src>/api newbug.c liblzma.a -o newbug && ./newbug

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

static lzma_index *
make_stream(lzma_check check)
{
	lzma_index *i = lzma_index_init(NULL);
	if (i == NULL)
		abort();

	const lzma_stream_flags flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = check,
	};

	if (lzma_index_stream_flags(i, &flags) != LZMA_OK)
		abort();

	return i;
}

int
main(void)
{
	// Build a three-Stream Index where every Stream has a known Check.
	lzma_index *idx = make_stream(LZMA_CHECK_CRC32);
	lzma_index *second = make_stream(LZMA_CHECK_CRC64);
	lzma_index *third = make_stream(LZMA_CHECK_NONE);

	if (lzma_index_cat(idx, second, NULL) != LZMA_OK)
		abort();
	if (lzma_index_cat(idx, third, NULL) != LZMA_OK)
		abort();

	// Duplicate and query the combined Check bitmask.
	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL)
		abort();

	uint32_t checks = lzma_index_checks(copy);
	printf("checks = 0x%08x\n", checks);

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);
	return 0;
}
