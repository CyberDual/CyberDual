// SPDX-License-Identifier: 0BSD
//
// Duplicate a multi-Stream lzma_index whose Streams all have known
// Stream Flags. This is the workflow documented in lzma/index.h:
// lzma_index_stream_flags() is called for every Stream (before the
// Streams are concatenated with lzma_index_cat()) so that
// lzma_index_checks() can report the Check types of every Stream.
//
// On a fixed liblzma this prints the Check bitmask of the original and
// the duplicate and exits with status 0.

#include <stdio.h>
#include <stdlib.h>

#include "lzma.h"


static lzma_index *
make_stream(lzma_check check)
{
	lzma_index *i = lzma_index_init(NULL);
	if (i == NULL)
		exit(1);

	if (lzma_index_append(i, NULL, 5, 100) != LZMA_OK
			|| lzma_index_append(i, NULL, 6, 200) != LZMA_OK)
		exit(1);

	// Stream Flags of the last Stream of this lzma_index.
	lzma_stream_flags flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = check,
	};

	if (lzma_index_stream_flags(i, &flags) != LZMA_OK)
		exit(1);

	return i;
}


int
main(void)
{
	setbuf(stdout, NULL);

	lzma_index *a = make_stream(LZMA_CHECK_CRC32);
	lzma_index *b = make_stream(LZMA_CHECK_CRC64);

	if (lzma_index_cat(a, b, NULL) != LZMA_OK)
		return 1;

	const uint32_t wanted = (UINT32_C(1) << LZMA_CHECK_CRC32)
			| (UINT32_C(1) << LZMA_CHECK_CRC64);

	printf("original checks: 0x%X\n", (unsigned)lzma_index_checks(a));
	if (lzma_index_checks(a) != wanted)
		return 1;

	// Duplicating the two-Stream Index must keep the Check statistics.
	lzma_index *copy = lzma_index_dup(a, NULL);
	if (copy == NULL)
		return 1;

	printf("duplicate checks: 0x%X\n", (unsigned)lzma_index_checks(copy));

	const int ok = lzma_index_checks(copy) == wanted;

	lzma_index_end(copy, NULL);
	lzma_index_end(a, NULL);

	if (!ok) {
		printf("duplicate lost the Check statistics\n");
		return 1;
	}

	printf("no crash\n");
	return 0;
}
