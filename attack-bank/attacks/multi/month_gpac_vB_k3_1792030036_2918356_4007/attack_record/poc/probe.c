/*
 * GPAC AC-4 frame parser probe.
 *
 * Drives the public entry point gf_ac4_parser() (include/gpac/avparse.h) with a
 * hand-built AC-4 sync frame whose single presentation references one
 * non-channel-coded substream group holding one object-based substream
 * (gf_ac4_substream_info_obj) with the reserved n_objects_code value 6.
 *
 * Build: gcc probe.c -I<gpac>/include -o probe -fsanitize=address \
 *            -L<gpac>/bin/gcc -lgpac -Wl,-rpath,<gpac>/bin/gcc
 */
#include <stdio.h>
#include <string.h>

#include <gpac/tools.h>
#include <gpac/avparse.h>

/* Make any sanitizer finding a hard abort (SIGABRT) so the run status itself
   reports the defect; leaks are out of scope for this probe. */
const char *__asan_default_options(void)
{
	return "abort_on_error=1:detect_leaks=0";
}

const char *__ubsan_default_options(void)
{
	return "halt_on_error=1:abort_on_error=1:print_stacktrace=1";
}

typedef struct {
	unsigned char bytes[128];
	unsigned int bitpos;
} BitWriter;

static void bw_bits(BitWriter *bw, unsigned int value, unsigned int nbits)
{
	unsigned int i;
	for (i = 0; i < nbits; i++) {
		unsigned int bit = (value >> (nbits - 1 - i)) & 1;
		if (bit)
			bw->bytes[bw->bitpos / 8] |= (unsigned char)(0x80u >> (bw->bitpos % 8));
		bw->bitpos++;
	}
}

int main(void)
{
	BitWriter bw;
	GF_AC4Config cfg;
	u32 pos = 0;
	Bool ok;

	memset(&bw, 0, sizeof(bw));

	/* sync frame header: syncword 0xAC40, 16-bit frame size */
	bw_bits(&bw, 0xAC40, 16);
	bw_bits(&bw, 0x0020, 16);

	/* ac4_toc */
	bw_bits(&bw, 2, 2);          /* bitstream_version = 2 */
	bw_bits(&bw, 0, 10);         /* sequence_counter */
	bw_bits(&bw, 0, 1);          /* b_wait_frames */
	bw_bits(&bw, 0, 1);          /* fs_index = 0 (48 kHz) */
	bw_bits(&bw, 1, 4);          /* frame_rate_index */
	bw_bits(&bw, 0, 1);          /* b_iframe_global */
	bw_bits(&bw, 1, 1);          /* b_single_presentation */
	bw_bits(&bw, 0, 1);          /* b_payload_base */
	bw_bits(&bw, 0, 1);          /* b_program_id */

	/* ac4_presentation_v1_info */
	bw_bits(&bw, 1, 1);          /* b_single_substream_group */
	bw_bits(&bw, 0, 1);          /* presentation_version terminator (b_tmp) */
	bw_bits(&bw, 0, 3);          /* md_compat */
	bw_bits(&bw, 0, 1);          /* b_presentation_id */
	bw_bits(&bw, 0, 1);          /* b_multiplier */
	bw_bits(&bw, 0, 2);          /* emdf_version */
	bw_bits(&bw, 0, 3);          /* key_id */
	bw_bits(&bw, 0, 1);          /* b_emdf_payloads_substream_info */
	bw_bits(&bw, 0, 2);          /* n_skip_bytes_length_primary */
	bw_bits(&bw, 0, 2);          /* n_skip_bytes_length_secondary */
	bw_bits(&bw, 0, 1);          /* b_presentation_filter */
	bw_bits(&bw, 0, 3);          /* group_index of the single substream group */
	bw_bits(&bw, 0, 1);          /* b_pre_virtualized */
	bw_bits(&bw, 0, 1);          /* b_add_emdf_substreams */
	bw_bits(&bw, 0, 1);          /* b_alternative */
	bw_bits(&bw, 0, 1);          /* b_pres_ndot */
	bw_bits(&bw, 0, 2);          /* substream_index */

	/* ac4_substream_group_info: non-channel-coded, one substream */
	bw_bits(&bw, 0, 1);          /* b_substreams_present */
	bw_bits(&bw, 0, 1);          /* b_hsf_ext */
	bw_bits(&bw, 1, 1);          /* b_single_substream */
	bw_bits(&bw, 0, 1);          /* b_channel_coded = 0 */
	bw_bits(&bw, 0, 1);          /* b_oamd_substream */
	bw_bits(&bw, 0, 1);          /* b_ajoc = 0 -> gf_ac4_substream_info_obj */

	/* ac4_substream_info_obj */
	bw_bits(&bw, 6, 3);          /* n_objects_code = 6 (reserved) */

	/* trailing zeros: b_dynamic_objects = 0, b_bed_objects = 0, b_isf = 0, ... */

	memset(&cfg, 0, sizeof(cfg));
	ok = gf_ac4_parser(bw.bytes, sizeof(bw.bytes), &pos, &cfg, GF_TRUE, GF_FALSE);
	gf_odf_ac4_cfg_clean_list(&cfg);

	printf("gf_ac4_parser -> %d (%u bits consumed)\n", ok, bw.bitpos);
	return 0;
}
