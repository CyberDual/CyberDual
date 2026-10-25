#include <gpac/ietf.h>
#include <gpac/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	(void) udta;
	(void) payload;
	(void) size;
	(void) hdr;
	(void) e;
}

int main(void)
{
	char sdp_text[] =
		"v=0\r\n"
		"o=- 0 0 IN IP4 127.0.0.1\r\n"
		"s=ttxt\r\n"
		"c=IN IP4 0.0.0.0\r\n"
		"t=0 0\r\n"
		"m=text 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 sver=60; width=176; height=144; tx=0; ty=0; layer=0; max-w=176; max-h=144; tx3g=AA==\r\n";
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *depacketizer;
	GF_RTPHeader hdr;
	u8 *payload;
	GF_Err e;

	gf_sys_init(GF_MemTrackerNone, NULL);

	sdp = gf_sdp_info_new();
	if (!sdp) return 1;
	e = gf_sdp_info_parse(sdp, sdp_text, (u32) strlen(sdp_text));
	if (e) {
		fprintf(stderr, "sdp parse failed: %s\n", gf_error_to_string(e));
		return 1;
	}
	media = gf_list_get(sdp->media_desc, 0);
	depacketizer = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	if (!depacketizer) {
		fprintf(stderr, "depacketizer setup failed\n");
		return 1;
	}

	payload = gf_malloc(8);
	if (!payload) return 1;
	payload[0] = 0x03;
	payload[1] = 0x00;
	payload[2] = 0x08;
	payload[3] = 0x11;
	payload[4] = 0x00;
	payload[5] = 0x00;
	payload[6] = 0x64;
	payload[7] = 0x01;

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.PayloadType = 96;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;
	hdr.SSRC = 1;

	gf_rtp_depacketizer_process(depacketizer, &hdr, payload, 8);

	gf_rtp_depacketizer_del(depacketizer);
	gf_sdp_info_del(sdp);
	gf_free(payload);
	gf_sys_close();
	return 0;
}
