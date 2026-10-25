#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	lzma_index *source = lzma_index_init(NULL);
	if (source == NULL)
		return 2;

	uint8_t encoded[16] = { 0x7F };
	size_t encoded_size = 1;
	if (lzma_index_buffer_encode(source, encoded, &encoded_size,
				sizeof(encoded)) != LZMA_OK) {
		lzma_index_end(source, NULL);
		return 2;
	}
	lzma_index_end(source, NULL);

	lzma_index *decoded = NULL;
	uint64_t memlimit = UINT64_C(1) << 20;
	size_t pos = 1;
	if (lzma_index_buffer_decode(&decoded, &memlimit, NULL,
			encoded, &pos, encoded_size) != LZMA_OK
			|| pos != encoded_size) {
		lzma_index_end(decoded, NULL);
		return 2;
	}
	lzma_index_end(decoded, NULL);

	if (encoded_size != 9)
		return 2;
	uint8_t *crafted = malloc(encoded_size);
	if (crafted == NULL)
		return 2;
	memcpy(crafted, encoded, encoded_size);
	crafted[2] = 2;
	crafted[3] = 5;
	crafted[4] = 0;
	crafted[5] = 5;
	crafted[6] = 0;
	crafted[7] = 0;
	crafted[8] = 0;
	pos = 1;
	memlimit = UINT64_C(1) << 20;
	const lzma_ret ret = lzma_index_buffer_decode(&decoded, &memlimit,
			NULL, crafted, &pos, encoded_size);
	lzma_index_end(decoded, NULL);
	free(crafted);
	return ret == LZMA_DATA_ERROR ? 0 : 3;
}
