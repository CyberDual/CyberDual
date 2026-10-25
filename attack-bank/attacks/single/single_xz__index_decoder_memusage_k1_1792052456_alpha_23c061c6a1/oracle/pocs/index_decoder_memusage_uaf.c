// SPDX-License-Identifier: 0BSD
//
// PoC: heap-use-after-free in the Index decoder's memory usage
// reporting (lzma_memusage() on a stream initialized with
// lzma_index_decoder()).
//
// A valid .xz Index is decoded incrementally with lzma_index_decoder()
// and lzma_code(). On LZMA_STREAM_END the decoded lzma_index becomes
// owned by the application, which frees it with lzma_index_end() as the
// API documentation instructs. The decoder (lzma_stream) is still
// initialized at that point, so calling lzma_memusage() on it must be
// safe. Instead, the Index decoder's memconfig handler dereferences the
// already-freed lzma_index.

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>


/// Encode a variable-length integer (xz "VLI", 7 bits per byte,
/// the highest bit set on all but the last byte).
static size_t
put_vli(uint8_t *buf, size_t pos, uint64_t v)
{
	while (v >= 0x80) {
		buf[pos++] = (uint8_t)(v | 0x80);
		v >>= 7;
	}

	buf[pos++] = (uint8_t)v;
	return pos;
}


/// Build a valid .xz Index field with three Records:
///
///   Index Indicator (0x00)
///   Number of Records (3)
///   Record: Unpadded Size, Uncompressed Size   (x3)
///   Index Padding (0x00 bytes)
///   CRC32
static size_t
make_index(uint8_t *buf)
{
	static const uint64_t unpadded[3] = { 5, 0x1000, 0x2000 };
	static const uint64_t uncompressed[3] = { 0, 1, 2 };

	size_t pos = 0;

	buf[pos++] = 0x00;
	pos = put_vli(buf, pos, 3);

	for (int i = 0; i < 3; ++i) {
		pos = put_vli(buf, pos, unpadded[i]);
		pos = put_vli(buf, pos, uncompressed[i]);
	}

	while ((pos & 3) != 0)
		buf[pos++] = 0x00;

	const uint32_t crc = lzma_crc32(buf, pos, 0);
	buf[pos++] = (uint8_t)(crc);
	buf[pos++] = (uint8_t)(crc >> 8);
	buf[pos++] = (uint8_t)(crc >> 16);
	buf[pos++] = (uint8_t)(crc >> 24);

	return pos;
}


int
main(void)
{
	uint8_t buf[64];
	const size_t size = make_index(buf);

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *idx = NULL;

	if (lzma_index_decoder(&strm, &idx, UINT64_MAX) != LZMA_OK)
		return 1;

	// Decode the Index incrementally, one input byte per lzma_code()
	// call, so that Records are decoded over several calls.
	lzma_ret ret = LZMA_OK;
	for (size_t i = 0; i < size && ret == LZMA_OK; ++i) {
		strm.next_in = buf + i;
		strm.avail_in = 1;
		ret = lzma_code(&strm, LZMA_RUN);
	}

	// The Index is valid, so decoding must have succeeded.
	if (ret != LZMA_STREAM_END)
		return 1;

	if (lzma_index_block_count(idx) != 3)
		return 1;

	// LZMA_STREAM_END transferred the ownership of the lzma_index to
	// the application, so free it as the API docs say.
	lzma_index_end(idx, NULL);

	// The decoder is still initialized; asking for its memory usage
	// estimate must not access the Index that was just freed.
	const uint64_t memusage = lzma_memusage(&strm);
	printf("lzma_memusage() returned %llu\n",
			(unsigned long long)memusage);

	lzma_end(&strm);
	return 0;
}
