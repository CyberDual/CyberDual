/*
 * GPAC CPIX key-info loader probe.
 *
 * Loads a CPIX document through the public gf_crypt_info_load() API (the
 * document is handed over as a registered in-memory blob, the same way the
 * crypt_tools unit tests drive the loader). The DRMSystemList entry carries a
 * version-1 PSSH payload that stops right after the 28-byte mandatory header.
 */
#include <stdio.h>
#include <string.h>

#include <gpac/setup.h>
#include <gpac/tools.h>
#include <gpac/list.h>
#include <gpac/crypt_tools.h>

static const char cpix_doc[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
	"<cpix:CPIX version=\"2.3\" xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
	"<cpix:ContentKeyList>"
	"<cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
	"<cpix:Data><pskc:Secret><pskc:PlainValue>Fzsa6fC/yLr6IPmeug0H2Q==</pskc:PlainValue></pskc:Secret></cpix:Data>"
	"</cpix:ContentKey>"
	"</cpix:ContentKeyList>"
	"<cpix:DRMSystemList>"
	"<cpix:DRMSystem kid=\"01234567-8901-2345-6789-012345678901\" systemId=\"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\">"
	"<cpix:PSSH>AAAAHHBzc2gBAAAA7e+LqXnWSs6jyCfc1R0h7Q==</cpix:PSSH>"
	"</cpix:DRMSystem>"
	"</cpix:DRMSystemList>"
	"</cpix:CPIX>";

int main(void)
{
	GF_Blob blob;
	char *blob_url;
	GF_Err e = GF_OK;
	GF_CryptInfo *info;

	gf_sys_init(GF_MemTrackerSimple, NULL);

	memset(&blob, 0, sizeof(blob));
	blob.data = (u8 *) cpix_doc;
	blob.size = (u32) (sizeof(cpix_doc) - 1);
	blob_url = gf_blob_register(&blob);
	if (!blob_url) {
		fprintf(stderr, "probe: blob registration failed\n");
		gf_sys_close();
		return 1;
	}

	info = gf_crypt_info_load(blob_url, &e);
	if (info) {
		GF_CryptDRMInfo *drm = (GF_CryptDRMInfo *) gf_list_get(info->drm_infos, 0);
		printf("probe: document loaded (tcis=%d drm_infos=%d)\n",
			gf_list_count(info->tcis), gf_list_count(info->drm_infos));
		if (drm)
			printf("probe: pssh version=%u nb_kids=%u private_data_size=%u\n",
				drm->version, drm->nb_kids, drm->private_data_size);
		gf_crypt_info_del(info);
	} else {
		printf("probe: document rejected, err=%d (%s)\n", (int) e, gf_error_to_string(e));
	}

	gf_blob_unregister(&blob);
	gf_free(blob_url);
	gf_sys_close();
	return 0;
}
