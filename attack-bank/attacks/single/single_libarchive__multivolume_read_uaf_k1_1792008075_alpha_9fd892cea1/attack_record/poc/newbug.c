#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SPLIT_AT (64 * 1024)
#define BODY_N   70000

static int
write_ustar(const char *path)
{
	struct archive *aw;
	struct archive_entry *e;
	unsigned char *body;
	int r;

	body = malloc(BODY_N);
	if (body == NULL)
		return (-1);
	memset(body, 'A', BODY_N);

	aw = archive_write_new();
	archive_write_set_format_ustar(aw);
	r = archive_write_open_filename(aw, path);
	if (r != ARCHIVE_OK) {
		archive_write_free(aw);
		free(body);
		return (-1);
	}
	e = archive_entry_new();
	archive_entry_set_pathname(e, "file.txt");
	archive_entry_set_size(e, BODY_N);
	archive_entry_set_filetype(e, AE_IFREG);
	archive_entry_set_perm(e, 0644);
	archive_write_header(aw, e);
	archive_write_data(aw, body, BODY_N);
	archive_entry_free(e);
	archive_write_close(aw);
	archive_write_free(aw);
	free(body);
	return (0);
}

static int
split_file(const char *src, const char *vol0, const char *vol1)
{
	FILE *in, *a, *b;
	unsigned char *buf;
	long n;

	in = fopen(src, "rb");
	if (in == NULL)
		return (-1);
	if (fseek(in, 0, SEEK_END) != 0) {
		fclose(in);
		return (-1);
	}
	n = ftell(in);
	if (n < SPLIT_AT) {
		fclose(in);
		return (-1);
	}
	if (fseek(in, 0, SEEK_SET) != 0) {
		fclose(in);
		return (-1);
	}
	buf = malloc((size_t)n);
	if (buf == NULL) {
		fclose(in);
		return (-1);
	}
	if (fread(buf, 1, (size_t)n, in) != (size_t)n) {
		free(buf);
		fclose(in);
		return (-1);
	}
	fclose(in);

	a = fopen(vol0, "wb");
	b = fopen(vol1, "wb");
	if (a == NULL || b == NULL) {
		if (a != NULL)
			fclose(a);
		if (b != NULL)
			fclose(b);
		free(buf);
		return (-1);
	}
	if (fwrite(buf, 1, SPLIT_AT, a) != SPLIT_AT ||
	    fwrite(buf + SPLIT_AT, 1, (size_t)n - SPLIT_AT, b) !=
	    (size_t)n - SPLIT_AT) {
		fclose(a);
		fclose(b);
		free(buf);
		return (-1);
	}
	fclose(a);
	fclose(b);
	free(buf);
	return (0);
}

int
main(void)
{
	char dir[] = "/tmp/la-mv-XXXXXX";
	char full[128], vol0[128], vol1[128];
	const char *names[3];
	struct archive *ar;
	struct archive_entry *entry;
	char buf[4096];
	la_ssize_t got;
	int r;

	if (mkdtemp(dir) == NULL)
		return (1);
	snprintf(full, sizeof(full), "%s/full.tar", dir);
	snprintf(vol0, sizeof(vol0), "%s/vol0", dir);
	snprintf(vol1, sizeof(vol1), "%s/vol1", dir);
	if (write_ustar(full) != 0 || split_file(full, vol0, vol1) != 0)
		return (1);

	names[0] = vol0;
	names[1] = vol1;
	names[2] = NULL;

	ar = archive_read_new();
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	r = archive_read_open_filenames(ar, names, 10240);
	if (r != ARCHIVE_OK) {
		archive_read_free(ar);
		return (1);
	}
	while ((r = archive_read_next_header(ar, &entry)) == ARCHIVE_OK) {
		while ((got = archive_read_data(ar, buf, sizeof(buf))) > 0)
			;
		if (got < 0)
			break;
	}
	archive_read_free(ar);
	unlink(full);
	unlink(vol0);
	unlink(vol1);
	rmdir(dir);
	return (r == ARCHIVE_EOF) ? 0 : 1;
}
