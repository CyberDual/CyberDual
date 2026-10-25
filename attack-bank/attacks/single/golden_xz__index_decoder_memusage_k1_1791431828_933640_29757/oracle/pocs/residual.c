#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *source = lzma_index_init(NULL);
	if (source == NULL || lzma_index_append(source, NULL, 5, 1) != LZMA_OK)
		return 1;

	const size_t size = (size_t)lzma_index_size(source);
	uint8_t *buffer = malloc(size);
	if (buffer == NULL)
		return 1;

	size_t pos = 0;
	if (lzma_index_buffer_encode(source, buffer, &pos, size) != LZMA_OK)
		return 1;
	lzma_index_end(source, NULL);

	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&stream, &decoded, UINT64_MAX) != LZMA_OK)
		return 1;

	stream.next_in = buffer;
	stream.avail_in = pos;
	if (lzma_code(&stream, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	lzma_index_end(decoded, NULL);
	printf("%llu\n", (unsigned long long)lzma_memusage(&stream));
	lzma_end(&stream);
	free(buffer);
	return 0;
}
