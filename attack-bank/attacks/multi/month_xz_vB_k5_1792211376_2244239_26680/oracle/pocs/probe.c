// SPDX-License-Identifier: 0BSD
//
// Proof-of-concept: drive liblzma's public Index API to combine several
// single-Stream lzma_index structures into one multi-Stream lzma_index
// and then destroy it. This is the same operation that applications
// perform when they collect information about concatenated .xz files
// (xz --list does this internally via lzma_file_info_decoder()).

#include <lzma.h>

// Ask AddressSanitizer to terminate the process with SIGABRT when it
// detects a memory error.
const char *__asan_default_options(void)
{
	return "abort_on_error=1";
}

int main(void)
{
	lzma_index *combined = lzma_index_init(NULL);
	if (combined == NULL)
		return 1;

	// One Block in the Stream.
	if (lzma_index_append(combined, NULL, LZMA_VLI_C(5), 1) != LZMA_OK)
		return 1;

	// Concatenate two more single-Stream Indexes. lzma_index_cat()
	// takes ownership of the second argument and frees it.
	for (unsigned k = 0; k < 2; k++) {
		lzma_index *one = lzma_index_init(NULL);
		if (one == NULL)
			return 1;

		if (lzma_index_append(one, NULL, LZMA_VLI_C(5), 1) != LZMA_OK)
			return 1;

		if (lzma_index_cat(combined, one, NULL) != LZMA_OK)
			return 1;
	}

	lzma_index_end(combined, NULL);
	return 0;
}
