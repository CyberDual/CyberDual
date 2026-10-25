/*
 * CPIX DRM config loader probe.
 *
 * Drives the public gpac DRM configuration parser (libgpac):
 *   gf_crypt_info_load() -> cryptinfo_load_cpix() -> gf_crypt_info_del()
 *
 * Input is a regular CPIX 2.x document (the format gpac documents for
 * `-crypt`/MP4Box encryption), carrying one FairPlay DRMSystem whose
 * HLSSignalingData is authored as a raw #EXT-X-KEY line inside a CDATA
 * section instead of the base64 wrapper.
 */
#include <stdio.h>
#include <string.h>

#include <gpac/crypt_tools.h>

static const char cpix_doc[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<cpix:CPIX version=\"2.3\" xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">\n"
	"<cpix:ContentKeyList>"
	"<cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cbcs\">"
	"<cpix:Data><pskc:Secret><pskc:PlainValue>Fzsa6fC/yLr6IPmeug0H2Q==</pskc:PlainValue></pskc:Secret></cpix:Data>"
	"</cpix:ContentKey></cpix:ContentKeyList>\n"
	"<cpix:DRMSystemList>"
	"<cpix:DRMSystem kid=\"01234567-8901-2345-6789-012345678901\" systemId=\"94ce86fb-07ff-4f43-adb8-93d2fa968ca2\">"
	"<cpix:HLSSignalingData playlist=\"media\"><![CDATA[#EXT-X-KEY:METHOD=SAMPLE-AES,URI=\"skd://01234567-8901-2345-6789-012345678901\",KEYFORMAT=\"com.apple.streamingkeydelivery\",KEYFORMATVERSIONS=\"1\"]]></cpix:HLSSignalingData>"
	"</cpix:DRMSystem>"
	"</cpix:DRMSystemList>\n"
	"</cpix:CPIX>\n";

int main(void)
{
	const char *path = "probe_cpix.xml";
	GF_Err e = GF_OK;
	GF_CryptInfo *info;
	FILE *f;

	f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "cannot write %s\n", path);
		return 2;
	}
	fwrite(cpix_doc, 1, strlen(cpix_doc), f);
	fclose(f);

	info = gf_crypt_info_load(path, &e);
	printf("gf_crypt_info_load: error=%d info=%p\n", (int) e, (void *) info);

	if (info) {
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *) gf_list_get(info->tcis, 0);
		if (tci && tci->keys && tci->keys[0].hls_info) {
			/* the HLS playlist writer reads this signaling line when the
			   encryption filter publishes GF_PROP_PID_HLS_KMS */
			size_t len = strlen(tci->keys[0].hls_info);
			unsigned long sum = 0;
			size_t i;
			for (i = 0; i < len; i++)
				sum += (unsigned char) tci->keys[0].hls_info[i];
			printf("hls_info len=%lu sum=%lu\n", (unsigned long) len, sum);
		} else {
			printf("no hls_info\n");
		}
		gf_crypt_info_del(info);
	}

	remove(path);
	printf("probe done\n");
	return 0;
}
