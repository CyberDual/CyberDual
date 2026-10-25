#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
put_u16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void
put_u32(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
	p[2] = (unsigned char)((v >> 16) & 0xff);
	p[3] = (unsigned char)((v >> 24) & 0xff);
}

/* Windows FILETIME: 100 ns ticks since 1601-01-01. */
static void
put_filetime(unsigned char *p, int64_t unix_sec, uint32_t nsec)
{
	uint64_t ticks = (uint64_t)(unix_sec + 11644473600LL) * 10000000ULL
	    + (nsec / 100);
	int i;

	for (i = 0; i < 8; i++) {
		p[i] = (unsigned char)(ticks & 0xff);
		ticks >>= 8;
	}
}

/*
 * Stored ZIP entry "a" whose 0x000A NTFS extra field carries tag 0x0001
 * with a declared attribute length larger than the three FILETIMEs, and
 * with that many bytes actually present in the extra field.
 */
static size_t
build_zip(unsigned char *out, size_t cap)
{
	enum { ATTR = 64 };
	unsigned char extra[128];
	unsigned char *e = extra;
	unsigned datasize = 4 + 2 + 2 + ATTR;
	unsigned extra_len;
	unsigned char local[256];
	unsigned char central[256];
	size_t local_len, central_len, off;

	put_u16(e, 0x000a); e += 2;
	put_u16(e, datasize); e += 2;
	memset(e, 0, 4); e += 4;
	put_u16(e, 0x0001); e += 2;
	put_u16(e, ATTR); e += 2;
	memset(e, 0, ATTR);
	put_filetime(e, 1700000000, 123456700);
	put_filetime(e + 8, 1600000000, 100);
	put_filetime(e + 16, 1500000000, 250000000);
	e += ATTR;
	extra_len = (unsigned)(e - extra);

	memset(local, 0, sizeof(local));
	memcpy(local, "PK\003\004", 4);
	put_u16(local + 4, 20);
	put_u16(local + 26, 1);
	put_u16(local + 28, extra_len);
	local[30] = 'a';
	memcpy(local + 31, extra, extra_len);
	local_len = 30 + 1 + extra_len;

	memset(central, 0, sizeof(central));
	memcpy(central, "PK\001\002", 4);
	central[4] = 20;
	central[5] = 3;
	put_u16(central + 6, 20);
	put_u16(central + 28, 1);
	put_u16(central + 30, extra_len);
	put_u32(central + 38, (unsigned)(0644 << 16));
	central[46] = 'a';
	memcpy(central + 47, extra, extra_len);
	central_len = 46 + 1 + extra_len;

	off = 0;
	if (local_len + central_len + 22 > cap)
		return 0;
	memcpy(out + off, local, local_len);
	off += local_len;
	memcpy(out + off, central, central_len);
	off += central_len;
	memcpy(out + off, "PK\005\006", 4);
	put_u16(out + off + 8, 1);
	put_u16(out + off + 10, 1);
	put_u32(out + off + 12, (unsigned)central_len);
	put_u32(out + off + 16, (unsigned)local_len);
	off += 22;
	return off;
}

int
main(void)
{
	unsigned char buf[512];
	size_t n;
	struct archive *a;
	struct archive_entry *entry;
	int r;

	n = build_zip(buf, sizeof(buf));
	if (n == 0)
		return 2;
	a = archive_read_new();
	if (a == NULL)
		return 2;
	archive_read_support_format_zip(a);
	r = archive_read_open_memory(a, buf, n);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open: %s\n", archive_error_string(a));
		archive_read_free(a);
		return 1;
	}
	r = archive_read_next_header(a, &entry);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "header: %s\n", archive_error_string(a));
		archive_read_free(a);
		return 1;
	}
	archive_read_free(a);
	return 0;
}
