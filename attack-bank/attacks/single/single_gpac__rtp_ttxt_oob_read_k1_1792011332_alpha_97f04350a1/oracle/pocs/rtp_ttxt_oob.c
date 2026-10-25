/*
 * 3GPP timed text (RFC 4396) RTP depacketizer check.
 *
 * Drives one complete timed text sample - a single type 2 (text segment) TTU
 * filling the whole RTP payload - through the public SDP + RTP depacketizer
 * API, the same way the rtpin filter sets the depacketizer up from an SDP.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gpac/ietf.h>

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *sl_hdr, GF_Err e)
{
	/*no downstream SL consumer in this test*/
}

#ifdef __SANITIZE_ADDRESS__
const char *__asan_default_options(void)
{
	/*report memory errors as aborts, skip exit-time leak checks*/
	return "abort_on_error=1:detect_leaks=0";
}
#endif

static const char *sdp_text =
	"v=0\r\n"
	"o=- 0 0 IN IP4 127.0.0.1\r\n"
	"s=3gpp-tt\r\n"
	"c=IN IP4 127.0.0.1\r\n"
	"t=0 0\r\n"
	"m=text 4000 RTP/AVP 96\r\n"
	"a=rtpmap:96 3gpp-tt/1000\r\n"
	"a=fmtp:96 tx=20;ty=20;width=160;height=60;layer=0;tx3g=AAECAwQFBgcICQ==\r\n";

int main(void)
{
	GF_Err e;
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtpd;
	GF_RTPHeader hdr;
	u8 *payload;
	u32 size, txt_len;
	const char *txt = "Hello world";

	gf_sys_init(GF_MemTrackerSimple, NULL);

	sdp = gf_sdp_info_new();
	e = gf_sdp_info_parse(sdp, (char *) sdp_text, (u32) strlen(sdp_text));
	if (e != GF_OK) {
		fprintf(stderr, "SDP parse failed: %d\n", e);
		return 1;
	}
	media = (GF_SDPMedia *) gf_list_get(sdp->media_desc, 0);
	if (!media) {
		fprintf(stderr, "no media in SDP\n");
		return 1;
	}

	rtpd = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	if (!rtpd) {
		fprintf(stderr, "depacketizer creation failed\n");
		return 1;
	}

	/*one complete sample in a single TTU: 3 byte TTU header, 7 byte type 2
	  header, then the text string; the TTU spans the entire payload*/
	txt_len = (u32) strlen(txt);
	size = 10 + txt_len;
	payload = (u8 *) malloc(size);
	if (!payload) return 1;

	payload[0] = 0x02;				/*UTF-8, TTU type 2 (text segment)*/
	payload[1] = (u8) ((size - 1) >> 8);	/*TTU length*/
	payload[2] = (u8) ((size - 1) & 0xFF);
	payload[3] = 0x10;				/*1 fragment, fragment 0*/
	payload[4] = 0x00;				/*sample duration, 1000*/
	payload[5] = 0x03;
	payload[6] = 0xE8;
	payload[7] = 0x00;				/*sample index*/
	payload[8] = (u8) (txt_len >> 8);		/*complete text sample size*/
	payload[9] = (u8) (txt_len & 0xFF);
	memcpy(payload + 10, txt, txt_len);

	memset(&hdr, 0, sizeof(hdr));
	hdr.PayloadType = 96;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;
	hdr.Marker = 1;

	gf_rtp_depacketizer_process(rtpd, &hdr, payload, size);

	free(payload);
	gf_rtp_depacketizer_del(rtpd);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
