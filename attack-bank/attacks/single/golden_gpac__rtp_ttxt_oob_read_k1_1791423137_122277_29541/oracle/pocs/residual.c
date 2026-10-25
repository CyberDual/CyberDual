#include <gpac/setup.h>
#include <gpac/ietf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void on_sl_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	u32 i;
	u8 sum = 0;
	(void) udta;
	(void) hdr;
	(void) e;
	/* mimic a real consumer (e.g. a sample writer) copying out the
	 * reported access unit bytes */
	for (i = 0; i < size; i++) sum += payload[i];
	if (sum == 0xFF) fprintf(stderr, "unreachable\n");
}

int main(void)
{
	GF_SDPMedia *media;
	GF_RTPMap *map;
	GF_SDP_FMTP *fmtp;
	GF_X_Attribute *att;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	u8 *payload;
	u32 size;

	media = gf_sdp_media_new();
	media->Type = 3;
	media->Profile = gf_strdup("RTP/AVP");

	map = (GF_RTPMap*)gf_malloc(sizeof(GF_RTPMap));
	memset(map, 0, sizeof(GF_RTPMap));
	map->PayloadType = 98;
	map->payload_name = gf_strdup("3gpp-tt");
	map->ClockRate = 1000;
	gf_list_add(media->RTPMaps, map);

	fmtp = gf_sdp_fmtp_new();
	fmtp->PayloadType = 98;
	att = (GF_X_Attribute*)gf_malloc(sizeof(GF_X_Attribute));
	att->Name = gf_strdup("tx3g");
	att->Value = gf_strdup("AAAAAAAAAAAAAAAAAAAA");
	gf_list_add(fmtp->Attributes, att);
	gf_list_add(media->FMTP, fmtp);

	rtp = gf_rtp_depacketizer_new(media, 0, on_sl_packet, NULL);
	if (!rtp) {
		fprintf(stderr, "failed to create depacketizer\n");
		return 1;
	}

	size = 16;
	payload = (u8*) gf_malloc(size);
	memset(payload, 0, size);
	/* first byte: is_utf_16(1 bit)=0, reserved(4 bits)=0, type(3 bits)=1 -> 0x01 */
	payload[0] = 0x01;
	/* ttu_len (u16, big endian): advertise the full buffer size, one byte
	 * too large once the implicit trailing byte read by the type==1 path
	 * is taken into account */
	payload[1] = (size >> 8) & 0xFF;
	payload[2] = size & 0xFF;

	memset(&hdr, 0, sizeof(hdr));
	hdr.PayloadType = 98;
	hdr.TimeStamp = 1000;
	hdr.Marker = 1;

	gf_rtp_depacketizer_process(rtp, &hdr, payload, size);

	gf_free(payload);
	gf_rtp_depacketizer_del(rtp);

	return 0;
}
