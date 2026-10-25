#include <gpac/crypt_tools.h>
#include <gpac/tools.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	static const char cpix[] =
		"<?xml version=\"1.0\"?>"
		"<cpix:CPIX xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
		"<cpix:ContentKeyList><cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" "
		"explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
		"<cpix:Data><pskc:Secret><pskc:PlainValue><![CDATA[ Fzsa6fC/yLr6IPmeug0H2Q==]]></pskc:PlainValue>"
		"</pskc:Secret></cpix:Data></cpix:ContentKey></cpix:ContentKeyList></cpix:CPIX>";
	GF_Blob blob;
	GF_CryptInfo *info;
	GF_Err err = GF_OK;
	char *url;

	memset(&blob, 0, sizeof(blob));
	blob.data = (u8 *) cpix;
	blob.size = sizeof(cpix) - 1;
	url = gf_blob_register(&blob);
	if (!url) return 2;
	info = gf_crypt_info_load(url, &err);
	if (info) gf_crypt_info_del(info);
	gf_blob_unregister(&blob);
	gf_free(url);
	if (err || !info) {
		fprintf(stderr, "CPIX load failed: %d\n", err);
		return 3;
	}
	return 0;
}
