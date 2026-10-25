/*
 * Test client for the 3GPP timed text RTP depacketizer.
 *
 * Sets up a GF_RTPDepacketizer for the "3gpp-tt" payload format from an SDP
 * description, then feeds it an RTP payload and consumes the reassembled
 * MPEG-4 text access units.
 *
 * Usage: ttxt_depack [payload.bin]
 *   Without argument, an embedded sample payload is used.
 */
#include <gpac/ietf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sdp_text =
	"v=0\r\n"
	"o=- 0 0 IN IP4 127.0.0.1\r\n"
	"s=gpac timed text rtp test\r\n"
	"c=IN IP4 127.0.0.1\r\n"
	"t=0 0\r\n"
	"m=text 5004 RTP/AVP 96\r\n"
	"a=rtpmap:96 3gpp-tt/1000\r\n"
	"a=fmtp:96 tx3g=AAAA\r\n";

/* sample payload: a TYPE 1 TTU (complete sample "Hello, ") followed by a
   TYPE 2 TTU (text fragment "world!") in the same RTP packet */
static const u8 sample_payload[] = {
	0x01, 0x00, 0x0D, 0x00, 0x00, 0x03, 0xE8,
	'H', 'e', 'l', 'l', 'o', ',', ' ',
	0x02, 0x00, 0x0F, 0x11, 0x00, 0x03, 0xE8, 0x00, 0x00, 0x06,
	'w', 'o', 'r', 'l', 'd', '!'
};

typedef struct
{
	u8 *au_buf;
	u32 au_buf_size;
	u32 nb_au;
	u64 total_bytes;
} TTClient;

static void on_sl_packet(void *udta, u8 *data, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	TTClient *tt = (TTClient *)udta;
	u32 to_copy = size;
	(void)hdr;
	if (e != GF_OK) return;
	if (to_copy > tt->au_buf_size) to_copy = tt->au_buf_size;
	/* consume the access unit data as a decoder client would */
	memcpy(tt->au_buf, data, to_copy);
	tt->nb_au++;
	tt->total_bytes += size;
}

int main(int argc, char **argv)
{
	GF_SDPInfo *sdp;
	GF_SDPMedia *media;
	GF_RTPDepacketizer *rtp;
	GF_RTPHeader hdr;
	TTClient tt;
	u8 *payload;
	u32 payload_size;
	GF_Err e;

	payload_size = (u32)sizeof(sample_payload);
	payload = (u8 *)malloc(payload_size);
	if (!payload) return 1;
	memcpy(payload, sample_payload, payload_size);

	if (argc > 1) {
		FILE *f = fopen(argv[1], "rb");
		long fsz;
		if (!f) {
			fprintf(stderr, "cannot open %s\n", argv[1]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		fsz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (fsz <= 0) {
			fclose(f);
			fprintf(stderr, "empty payload file\n");
			return 1;
		}
		free(payload);
		payload = (u8 *)malloc((size_t)fsz);
		if (!payload || fread(payload, 1, (size_t)fsz, f) != (size_t)fsz) {
			fclose(f);
			fprintf(stderr, "cannot read payload file\n");
			return 1;
		}
		fclose(f);
		payload_size = (u32)fsz;
	}

	memset(&tt, 0, sizeof(tt));
	tt.au_buf_size = 4096;
	tt.au_buf = (u8 *)malloc(tt.au_buf_size);
	if (!tt.au_buf) return 1;

	sdp = gf_sdp_info_new();
	e = gf_sdp_info_parse(sdp, (char *)sdp_text, (u32)strlen(sdp_text));
	if (e) {
		fprintf(stderr, "failed to parse SDP: %d\n", e);
		return 1;
	}
	media = (GF_SDPMedia *)gf_list_get(sdp->media_desc, 0);
	if (!media) {
		fprintf(stderr, "no media in SDP\n");
		return 1;
	}

	rtp = gf_rtp_depacketizer_new(media, 0, on_sl_packet, &tt);
	if (!rtp) {
		fprintf(stderr, "failed to create depacketizer\n");
		return 1;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.Version = 2;
	hdr.PayloadType = 96;
	hdr.Marker = 1;
	hdr.SequenceNumber = 1;
	hdr.TimeStamp = 1000;

	gf_rtp_depacketizer_process(rtp, &hdr, payload, payload_size);

	printf("processed %u payload bytes, %u access units, %llu AU bytes\n",
	       payload_size, tt.nb_au, (unsigned long long)tt.total_bytes);

	gf_rtp_depacketizer_del(rtp);
	gf_sdp_info_del(sdp);
	free(tt.au_buf);
	free(payload);
	return 0;
}
