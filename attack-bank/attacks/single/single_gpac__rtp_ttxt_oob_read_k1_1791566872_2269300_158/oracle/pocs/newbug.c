#include <gpac/ietf.h>
#include <gpac/list.h>
#include <string.h>

static void on_sl(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
}

static const char *sdp_text =
	"v=0\r\n"
	"o=- 0 0 IN IP4 127.0.0.1\r\n"
	"s=-\r\n"
	"t=0 0\r\n"
	"m=text 0 RTP/AVP 98\r\n"
	"a=rtpmap:98 3gpp-tt/1000\r\n"
	"a=fmtp:98 tx3g=AAAAAAAA\r\n";

int main(int argc, char **argv)
{
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *dmx;
	GF_RTPHeader hdr;
	u32 size = 40;
	u8 *payload;
	u32 ttu_len = size - 1; /* single TTU fills the payload */

	gf_sys_init(GF_MemTrackerNone, NULL);

	sdp = gf_sdp_info_new();
	gf_sdp_info_parse(sdp, (char *) sdp_text, (u32) strlen(sdp_text));
	media = (GF_SDPMedia *) gf_list_get(sdp->media_desc, 0);

	dmx = gf_rtp_depacketizer_new(media, 0, on_sl, NULL);
	if (!dmx) return 1;

	payload = (u8 *) gf_malloc(size);
	memset(payload, 0, size);
	payload[0] = 0x02;                 /* is_utf16=0, reserved=0, type=2 */
	payload[1] = (u8) (ttu_len >> 8);  /* ttu_len high */
	payload[2] = (u8) (ttu_len & 0xFF);/* ttu_len low  */
	payload[3] = 0x10;                 /* nb_frag=1, cur_frag=0 */

	memset(&hdr, 0, sizeof(hdr));
	hdr.TimeStamp = 0;
	hdr.Marker = 0;

	gf_rtp_depacketizer_process(dmx, &hdr, payload, size);

	gf_free(payload);
	gf_rtp_depacketizer_del(dmx);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
