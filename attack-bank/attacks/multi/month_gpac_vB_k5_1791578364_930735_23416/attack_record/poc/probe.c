/*
 * PoC: gf_isom_cenc_get_default_info() reports GF_CENC_KEY_INFO_MAX_SIZE (37)
 * as the key-info size for a PIFF-protected track, but the backing buffer
 * (GF_PIFFTrackEncryptionBox.key_info) is only 20 bytes. Any consumer that
 * trusts the reported size when reading the buffer (as production code in
 * isoffin_read.c does via gf_crc_32()) reads past the end of the box.
 *
 * Public API only: gf_isom_open, gf_isom_new_track,
 * gf_isom_new_generic_sample_description, gf_isom_set_cenc_protection,
 * gf_isom_cenc_get_default_info, gf_crc_32.
 */
#include <gpac/isomedia.h>
#include <gpac/tools.h>
#include <string.h>

int main(void)
{
	/* Single-key PIFF key info: 3-byte header, 1-byte IV size, 16-byte KID. */
	struct {
		u8 prefix[3];
		u8 iv_size;
		u8 key_id[16];
	} key_info = {{0}, 8, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
	GF_GenericSampleDescription desc;
	GF_ISOFile *file;
	GF_Err e;
	u32 track, desc_index, returned_size;
	const u8 *returned_key_info;
	Bool encrypted;

	memset(&desc, 0, sizeof(desc));

	file = gf_isom_open("probe-piff.mp4", GF_ISOM_WRITE_EDIT, NULL);
	if (!file) return 1;

	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) { gf_isom_delete(file); return 1; }

	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	if (e) { gf_isom_delete(file); return 1; }

	/* Create PIFF protection through the public API, exactly as a muxer would. */
	e = gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_PIFF_SCHEME,
		0x00010000, GF_TRUE, 0, 0, (u8 *) &key_info, sizeof(key_info));
	if (e) { gf_isom_delete(file); return 1; }

	/* Same call an ISOBMFF reader makes when exposing CENC_KEY_INFO on a PID. */
	e = gf_isom_cenc_get_default_info(file, track, desc_index, NULL, &encrypted,
		NULL, NULL, &returned_key_info, &returned_size);
	if (e) { gf_isom_delete(file); return 1; }

	/* isoffin_read.c hashes the key info with this exact call once it has
	 * the (pointer, size) pair from gf_isom_cenc_get_default_info(). */
	gf_crc_32(returned_key_info, returned_size);

	gf_isom_delete(file);
	return 0;
}
