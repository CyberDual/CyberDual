#include <gpac/ietf.h>
#include <gpac/tools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_sl(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	u8 *copy;
	(void)udta;
	(void)hdr;
	(void)e;
	if (!payload || !size) return;
	copy = (u8 *)malloc(size);
	if (!copy) return;
	memcpy(copy, payload, size);
	free(copy);
}

/* One complete text sample (TTU type 1, empty text) followed by a text
 * fragment (TTU type 2) carrying two characters. Length fields match the
 * bytes actually present in the RTP payload.
 */
static const u8 kPayload[] = {
	0x01, 0x00, 0x08, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x00,
	0x02, 0x00, 0x0B, 0x10, 0x00, 0x00, 0x0A, 0x01, 0x00, 0x02,
	'H', 'i'
};

int main(void)
{
	const char *sdp_text =
		"v=0\r\n"
		"o=- 0 0 IN IP4 127.0.0.1\r\n"
		"s=ttxt\r\n"
		"c=IN IP4 127.0.0.1\r\n"
		"t=0 0\r\n"
		"m=text 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 sver=60; width=176; height=144; tx=0; ty=0; layer=0; max-w=176; max-h=144; tx3g=AAAA\r\n";
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	u8 *payload;
	GF_Err err;

	gf_sys_init(GF_MemTrackerNone, NULL);

	sdp = gf_sdp_info_new();
	if (!sdp) return 2;
	err = gf_sdp_info_parse(sdp, (char *)sdp_text, (u32)strlen(sdp_text));
	if (err != GF_OK) {
		fprintf(stderr, "sdp parse %d\n", err);
		return 2;
	}
	media = (GF_SDPMedia *)gf_list_get(sdp->media_desc, 0);
	rtp = gf_rtp_depacketizer_new(media, 0, on_sl, NULL);
	if (!rtp) {
		fprintf(stderr, "no depacketizer\n");
		return 2;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.PayloadType = 96;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;
	hdr.Marker = 0;

	payload = (u8 *)malloc(sizeof(kPayload));
	if (!payload) return 2;
	memcpy(payload, kPayload, sizeof(kPayload));

	gf_rtp_depacketizer_process(rtp, &hdr, payload, (u32)sizeof(kPayload));

	free(payload);
	gf_rtp_depacketizer_del(rtp);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
