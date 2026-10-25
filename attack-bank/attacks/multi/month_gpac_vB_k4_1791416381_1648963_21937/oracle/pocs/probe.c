#include <gpac/crypt_tools.h>
#include <gpac/tools.h>
#include <string.h>

int main(void)
{
	static const char cpix[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<cpix:CPIX xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
		"<cpix:ContentKeyList><cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" "
		"explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
		"<cpix:Data><pskc:Secret><pskc:PlainValue>Fzsa6fC/yLr6IPmeug0H2Q==</pskc:PlainValue>"
		"</pskc:Secret></cpix:Data></cpix:ContentKey></cpix:ContentKeyList></cpix:CPIX>";
	static const u8 expected_key[16] = {
		0x17, 0x3b, 0x1a, 0xe9, 0xf0, 0xbf, 0xc8, 0xba,
		0xfa, 0x20, 0xf9, 0x9e, 0xba, 0x0d, 0x07, 0xd9
	};
	GF_Blob blob = {0};
	GF_CryptInfo *info;
	GF_TrackCryptInfo *track;
	GF_Err e = GF_OK;
	char *url;
	int result = 0;

	if (gf_sys_init(GF_MemTrackerNone, NULL)) return 1;
	blob.data = (u8 *)cpix;
	blob.size = sizeof(cpix) - 1;
	url = gf_blob_register(&blob);
	if (!url) {
		gf_sys_close();
		return 2;
	}
	info = gf_crypt_info_load(url, &e);
	if (!info || e != GF_OK) {
		result = 3;
	} else {
		track = (GF_TrackCryptInfo *)gf_list_get(info->tcis, 0);
		if (!track || memcmp(track->keys[0].key, expected_key, sizeof(expected_key))) result = 4;
		gf_crypt_info_del(info);
	}
	gf_blob_unregister(&blob);
	gf_free(url);
	gf_sys_close();
	return result;
}
