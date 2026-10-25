#include <stdio.h>
#include <string.h>
#include <gpac/isomedia.h>
#include <gpac/tools.h>

/* Build a PIFF file through the public writer, then read back a versioned
   AES-CBC track-encryption box with subsample flags set. */
int main(void)
{
	const char *path = "/tmp/piff_probe.mp4";
	u8 kid[16];
	u8 pattern[24];
	struct {
		u8 prefix[3];
		u8 iv_size;
		u8 key_id[16];
	} key_info;
	GF_GenericSampleDescription desc;
	GF_ISOFile *file;
	GF_Err e;
	u32 track, desc_index = 0, key_info_size = 0;
	const u8 *returned = NULL;
	FILE *fp;
	long flen;
	u8 *buf;
	u32 i, found = 0;

	for (i = 0; i < 16; i++)
		kid[i] = (u8) (0xA0 + i);
	memset(&key_info, 0, sizeof(key_info));
	key_info.iv_size = 16;
	memcpy(key_info.key_id, kid, 16);

	gf_sys_init(GF_MemTrackerNone, NULL);
	remove(path);
	file = gf_isom_open(path, GF_ISOM_OPEN_WRITE, NULL);
	if (!file)
		return 2;
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		gf_isom_delete(file);
		return 2;
	}
	memset(&desc, 0, sizeof(desc));
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	if (e) {
		gf_isom_delete(file);
		return 2;
	}
	e = gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_PIFF_SCHEME,
		0x00010000, GF_TRUE, 0, 0, (u8 *) &key_info, (u32) sizeof(key_info));
	if (e) {
		gf_isom_delete(file);
		return 2;
	}
	e = gf_isom_close(file);
	if (e)
		return 2;

	fp = fopen(path, "rb");
	if (!fp)
		return 2;
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return 2;
	}
	flen = ftell(fp);
	if (flen < 24) {
		fclose(fp);
		return 2;
	}
	rewind(fp);
	buf = gf_malloc((u32) flen);
	if (!buf || fread(buf, 1, (size_t) flen, fp) != (size_t) flen) {
		fclose(fp);
		gf_free(buf);
		return 2;
	}
	fclose(fp);

	pattern[0] = 0x00;
	pattern[1] = 0x00;
	pattern[2] = 0x00;
	pattern[3] = 0x00;
	pattern[4] = 0x00;
	pattern[5] = 0x00;
	pattern[6] = 0x01;
	pattern[7] = 0x10;
	memcpy(pattern + 8, kid, 16);
	for (i = 0; i + 24 <= (u32) flen; i++) {
		if (!memcmp(buf + i, pattern, 24)) {
			buf[i] = 0x01;
			buf[i + 3] = 0x01;
			buf[i + 6] = 0x02;
			found = 1;
			break;
		}
	}
	if (!found) {
		gf_free(buf);
		return 3;
	}
	fp = fopen(path, "wb");
	if (!fp) {
		gf_free(buf);
		return 2;
	}
	if (fwrite(buf, 1, (size_t) flen, fp) != (size_t) flen) {
		fclose(fp);
		gf_free(buf);
		return 2;
	}
	fclose(fp);
	gf_free(buf);

	file = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
	if (!file)
		return 2;
	e = gf_isom_cenc_get_default_info(file, 1, 1, NULL, NULL, NULL, NULL, &returned, &key_info_size);
	if (e || !returned) {
		gf_isom_close(file);
		return 2;
	}
	(void) gf_cenc_key_info_get_iv_size(returned, key_info_size, 0, NULL, NULL);
	gf_isom_close(file);
	gf_sys_close();
	remove(path);
	return 0;
}
