#include <gpac/crypt_tools.h>
#include <gpac/isomedia.h>
#include <gpac/tools.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
	static const char cpix[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<cpix:CPIX version=\"2.3\" xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
		"<cpix:ContentKeyList>"
		"<cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
		"<cpix:Data><pskc:Secret><pskc:PlainValue>!!!!</pskc:PlainValue></pskc:Secret></cpix:Data>"
		"</cpix:ContentKey></cpix:ContentKeyList></cpix:CPIX>";
	GF_GenericSampleDescription description;
	GF_Blob blob;
	GF_ISOFile *input;
	GF_Err err;
	char *blob_url;
	u32 track;
	u32 description_index;

	gf_sys_init(GF_MemTrackerNone, NULL);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_QUIET);

	input = gf_isom_open("gpac-cpix-probe-input.mp4", GF_ISOM_WRITE_EDIT, NULL);
	if (!input) return 1;
	track = gf_isom_new_track(input, 1, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		gf_isom_delete(input);
		return 1;
	}
	memset(&description, 0, sizeof(description));
	description.codec_tag = GF_4CC('t', 'e', 's', 't');
	err = gf_isom_new_generic_sample_description(input, track, NULL, NULL, &description, &description_index);
	if (err || !description_index) {
		gf_isom_delete(input);
		return 1;
	}
	err = gf_isom_close(input);
	if (err) return 1;

	input = gf_isom_open("gpac-cpix-probe-input.mp4", GF_ISOM_OPEN_READ, NULL);
	if (!input) return 1;
	memset(&blob, 0, sizeof(blob));
	blob.data = (u8 *) cpix;
	blob.size = (u32) (sizeof(cpix) - 1);
	blob_url = gf_blob_register(&blob);
	if (!blob_url) {
		gf_isom_delete(input);
		return 1;
	}

	err = gf_crypt_file(input, blob_url, "gpac-cpix-probe-output.mp4", 0, 0);
	gf_isom_delete(input);
	gf_blob_unregister(&blob);
	gf_free(blob_url);
	gf_sys_close();
	return err ? 0 : 0;
}
