#include <stdio.h>
#include <lzma.h>

/* One Stream with more Records than a single index_group holds, then a
 * second Stream with a different Check type. Duplicating that Index is a
 * supported lzma_index_* use. */
int
main(void)
{
	lzma_index *small = lzma_index_init(NULL);
	lzma_index *small2 = lzma_index_init(NULL);
	if (small == NULL || small2 == NULL)
		return 1;

	lzma_stream_flags flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_CRC32,
	};

	if (lzma_index_append(small, NULL, 5, 1) != LZMA_OK)
		return 1;
	if (lzma_index_stream_flags(small, &flags) != LZMA_OK)
		return 1;

	flags.check = LZMA_CHECK_SHA256;
	if (lzma_index_append(small2, NULL, 5, 1) != LZMA_OK)
		return 1;
	if (lzma_index_stream_flags(small2, &flags) != LZMA_OK)
		return 1;
	if (lzma_index_cat(small, small2, NULL) != LZMA_OK)
		return 1;

	lzma_index *small_dup = lzma_index_dup(small, NULL);
	if (small_dup == NULL)
		return 1;
	if (lzma_index_checks(small_dup)
			!= (LZMA_INDEX_CHECK_MASK_CRC32
				| LZMA_INDEX_CHECK_MASK_SHA256))
		return 2;
	lzma_index_end(small_dup, NULL);
	lzma_index_end(small, NULL);

	lzma_index *idx = lzma_index_init(NULL);
	lzma_index *tail = lzma_index_init(NULL);
	if (idx == NULL || tail == NULL)
		return 1;

	for (unsigned i = 0; i < 513; ++i) {
		if (lzma_index_append(idx, NULL, 5, 1) != LZMA_OK)
			return 1;
	}

	flags.check = LZMA_CHECK_CRC32;
	if (lzma_index_stream_flags(idx, &flags) != LZMA_OK)
		return 1;

	flags.check = LZMA_CHECK_SHA256;
	if (lzma_index_append(tail, NULL, 5, 1) != LZMA_OK)
		return 1;
	if (lzma_index_stream_flags(tail, &flags) != LZMA_OK)
		return 1;
	if (lzma_index_cat(idx, tail, NULL) != LZMA_OK)
		return 1;

	lzma_index *dup = lzma_index_dup(idx, NULL);
	if (dup == NULL)
		return 1;

	if (lzma_index_checks(dup)
			!= (LZMA_INDEX_CHECK_MASK_CRC32
				| LZMA_INDEX_CHECK_MASK_SHA256))
		return 2;

	lzma_index_end(dup, NULL);
	lzma_index_end(idx, NULL);
	return 0;
}
