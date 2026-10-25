#include <gpac/ietf.h>
#include <gpac/list.h>
#include <stdlib.h>
#include <string.h>

static void on_packet(void *user, u8 *data, u32 size, GF_SLHeader *hdr, GF_Err error)
{
	(void) user;
	(void) data;
	(void) size;
	(void) hdr;
	(void) error;
}

int main(void)
{
	char description[] =
		"v=0\r\n"
		"o=- 0 0 IN IP4 127.0.0.1\r\n"
		"s=Text\r\n"
		"c=IN IP4 127.0.0.1\r\n"
		"t=0 0\r\n"
		"m=text 5004 RTP/AVP 96\r\n"
		"a=rtpmap:96 3gpp-tt/1000\r\n"
		"a=fmtp:96 tx3g=AA==\r\n";
	const u8 bytes[] = {
		0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		0x02, 0x00, 0x0a, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 'A'
	};
	GF_SDPInfo *sdp = gf_sdp_info_new();
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	u8 *payload;

	if (!sdp || gf_sdp_info_parse(sdp, description, sizeof(description) - 1)) return 1;
	media = gf_list_get(sdp->media_desc, 0);
	if (!media) return 2;
	rtp = gf_rtp_depacketizer_new(media, 0, on_packet, NULL);
	if (!rtp) return 3;
	payload = malloc(sizeof(bytes));
	if (!payload) return 4;
	memcpy(payload, bytes, sizeof(bytes));
	memset(&hdr, 0, sizeof(hdr));
	hdr.TimeStamp = 100;
	hdr.Marker = 1;
	gf_rtp_depacketizer_process(rtp, &hdr, payload, sizeof(bytes));
	free(payload);
	gf_rtp_depacketizer_del(rtp);
	gf_sdp_info_del(sdp);
	return 0;
}
