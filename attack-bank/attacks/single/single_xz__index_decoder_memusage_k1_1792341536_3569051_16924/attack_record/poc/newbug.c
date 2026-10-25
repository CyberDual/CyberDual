#include <lzma.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *
encode_index(size_t *size)
{
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL)
		return NULL;

	if (lzma_index_append(index, NULL, 5, 1) != LZMA_OK
			|| lzma_index_append(index, NULL, 9, 2) != LZMA_OK) {
		lzma_index_end(index, NULL);
		return NULL;
	}

	*size = (size_t)lzma_index_size(index);
	uint8_t *buffer = malloc(*size);
	size_t out_pos = 0;
	if (buffer != NULL && lzma_index_buffer_encode(
			index, buffer, &out_pos, *size) != LZMA_OK) {
		free(buffer);
		buffer = NULL;
	}

	lzma_index_end(index, NULL);
	return buffer;
}

int
main(void)
{
	size_t size = 0;
	uint8_t *buffer = encode_index(&size);
	if (buffer == NULL)
		return 1;

	lzma_index *index = NULL;
	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_index_decoder(&strm, &index, UINT64_MAX) != LZMA_OK)
		return 2;

	strm.next_in = buffer;
	strm.avail_in = size;
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 3;

	printf("before free: %" PRIu64 "\n", lzma_memusage(&strm));
	lzma_index_end(index, NULL);
	printf("after free: %" PRIu64 "\n", lzma_memusage(&strm));

	lzma_end(&strm);
	free(buffer);
	return 0;
}
