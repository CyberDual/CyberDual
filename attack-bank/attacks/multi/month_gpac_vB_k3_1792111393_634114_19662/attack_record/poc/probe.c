#include <gpac/isomedia.h>
#include <gpac/tools.h>

int main(void)
{
	u8 key_info[21 + 32];
	u32 i;
	GF_GenericSampleDescription desc = {0};
	GF_ISOFile *file;
	u32 track;
	u32 desc_index = 0;

	for (i = 0; i < (u32) sizeof(key_info); i++)
		key_info[i] = 0;
	for (i = 0; i < 16; i++)
		key_info[4 + i] = (u8) (1 + i);
	key_info[20] = 32;
	for (i = 0; i < 32; i++)
		key_info[21 + i] = (u8) i;

	if (gf_sys_init(GF_MemTrackerNone, NULL) != GF_OK)
		return 0;

	file = gf_isom_open("/tmp/piff-civ.mp4", GF_ISOM_WRITE_EDIT, NULL);
	if (!file) {
		gf_sys_close();
		return 0;
	}
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		gf_isom_delete(file);
		gf_sys_close();
		return 0;
	}
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	if (gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index) != GF_OK) {
		gf_isom_delete(file);
		gf_sys_close();
		return 0;
	}
	gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_PIFF_SCHEME,
		0x00010000, GF_TRUE, 0, 0, key_info, (u32) sizeof(key_info));
	gf_isom_delete(file);
	gf_sys_close();
	return 0;
}
