#include <gpac/isomedia.h>
#include <gpac/tools.h>
#include <string.h>

const char *__asan_default_options(void)
{
	return "abort_on_error=1";
}

int main(void)
{
	GF_ISOFile *file;
	GF_GenericSampleDescription desc;
	GF_Err e;
	u32 track, desc_index, returned_size;
	const u8 *returned_key_info;
	Bool encrypted;
	u8 key_info[29];
	u32 i;

	gf_sys_init(GF_MemTrackerNone, NULL);

	memset(&desc, 0, sizeof(desc));
	memset(key_info, 0, sizeof(key_info));
	/* Single key, constant IV: per-sample IV size 0, then KID, then an 8-byte constant IV. */
	for (i = 0; i < 16; i++)
		key_info[4 + i] = (u8) (i + 1);
	key_info[20] = 8;
	for (i = 0; i < 8; i++)
		key_info[21 + i] = (u8) (0xA0 + i);

	file = gf_isom_open("/tmp/poc-piff.mp4", GF_ISOM_WRITE_EDIT, NULL);
	if (!file)
		return 1;
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		gf_isom_delete(file);
		return 1;
	}
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	if (e) {
		gf_isom_delete(file);
		return 1;
	}
	e = gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_PIFF_SCHEME,
		0x00010000, GF_TRUE, 0, 0, key_info, (u32) sizeof(key_info));
	if (e) {
		gf_isom_delete(file);
		return 1;
	}
	returned_key_info = NULL;
	returned_size = 0;
	encrypted = GF_FALSE;
	e = gf_isom_cenc_get_default_info(file, track, desc_index, NULL, &encrypted,
		NULL, NULL, &returned_key_info, &returned_size);
	if (e || !returned_key_info) {
		gf_isom_delete(file);
		return 1;
	}
	gf_cenc_validate_key_info(returned_key_info, returned_size);
	gf_isom_delete(file);
	gf_sys_close();
	return 0;
}
