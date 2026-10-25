#include <gpac/tools.h>
#include <gpac/ietf.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void packet_cbk(void *udta, uint8_t *payload, uint32_t size, GF_SLHeader *hdr, GF_Err e)
{
	(void) udta;
	(void) payload;
	(void) size;
	(void) hdr;
	(void) e;
}

int main(void)
{
	const char *sdp_text =
		"v=0\r\n"
		"s=3GPP Timed Text\r\n"
		"m=text 49170 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 tx3g=AA==\r\n";
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *depacketizer;
	GF_RTPHeader header;
	uint8_t *payload;
	const uint32_t payload_size = 8;

	sdp = gf_sdp_info_new();
	if (!sdp || gf_sdp_info_parse(sdp, (char *) sdp_text, (uint32_t) strlen(sdp_text)) != GF_OK) {
		gf_sdp_info_del(sdp);
		return 1;
	}
	media = gf_list_get(sdp->media_desc, 0);
	depacketizer = media ? gf_rtp_depacketizer_new(media, 0, packet_cbk, NULL) : NULL;
	gf_sdp_info_del(sdp);
	if (!depacketizer) return 2;

	payload = malloc(payload_size);
	if (!payload) {
		gf_rtp_depacketizer_del(depacketizer);
		return 3;
	}
	payload[0] = 0x1b;
	payload[1] = 0x00;
	payload[2] = 0x20;
	payload[3] = 0x00;
	payload[4] = 0x00;
	payload[5] = 0x00;
	payload[6] = 0x00;
	payload[7] = 0x00;

	memset(&header, 0, sizeof(header));
	header.TimeStamp = 1;
	header.Marker = 1;
	gf_rtp_depacketizer_process(depacketizer, &header, payload, payload_size);

	free(payload);
	gf_rtp_depacketizer_del(depacketizer);
	return 0;
}
