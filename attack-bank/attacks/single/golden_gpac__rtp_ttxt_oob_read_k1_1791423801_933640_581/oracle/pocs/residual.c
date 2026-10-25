#include <gpac/ietf.h>
#include <gpac/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_packet(void *udta, u8 *data, u32 size, GF_SLHeader *hdr, GF_Err err)
{
	u8 *copy = malloc(size);
	if (!copy) exit(2);
	memcpy(copy, data, size);
	free(copy);
}

int main(void)
{
	char sdp_text[] = "v=0\r\n"
		"o=- 1 1 IN IP4 127.0.0.1\r\n"
		"s=Timed text\r\n"
		"t=0 0\r\n"
		"m=text 5004 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 tx3g=AA==\r\n";
	u8 *payload;
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *depacketizer;
	GF_RTPHeader header = {0};

	if (gf_sys_init(GF_MemTrackerNone, NULL)) return 2;
	sdp = gf_sdp_info_new();
	if (!sdp || gf_sdp_info_parse(sdp, sdp_text, strlen(sdp_text))) return 2;
	media = gf_list_get(sdp->media_desc, 0);
	if (!media) return 2;
	depacketizer = gf_rtp_depacketizer_new(media, 96, on_packet, NULL);
	if (!depacketizer) return 2;
	payload = malloc(8);
	if (!payload) return 2;
	memcpy(payload, "\x01\x00\x08\x01\x00\x00\x01\x00", 8);
	header.TimeStamp = 1000;
	gf_rtp_depacketizer_process(depacketizer, &header, payload, 8);
	free(payload);
	gf_rtp_depacketizer_del(depacketizer);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
