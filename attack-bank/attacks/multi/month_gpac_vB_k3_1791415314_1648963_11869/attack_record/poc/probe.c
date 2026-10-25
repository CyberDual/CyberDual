#include <gpac/filters.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	static const char cpix[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<cpix:CPIX version=\"2.3\" xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
		"<cpix:ContentKeyList><cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
		"<cpix:Data><pskc:Secret><pskc:PlainValue>Fzsa6fC/yLr6IPmeug0H2Q==</pskc:PlainValue></pskc:Secret></cpix:Data>"
		"</cpix:ContentKey></cpix:ContentKeyList>"
		"<cpix:DRMSystemList><cpix:DRMSystem systemId=\"94ce86fb-07ff-4f43-adb8-93d2fa968ca2\">"
		"<cpix:HLSSignalingData playlist=\"media\">I0VYVC1YLUtFWTpNRVRIT0Q9U0FNUExFLUFFUyxVUkk9c2tkOi8vZXhhbXBsZQ==</cpix:HLSSignalingData>"
		"</cpix:DRMSystem></cpix:DRMSystemList>"
		"</cpix:CPIX>";
	GF_FilterSession *session;
	GF_Filter *filter;
	GF_Err err = GF_OK;
	char path[] = "/tmp/gpac-cpix-XXXXXX";
	char args[256];
	int fd;

	fd = mkstemp(path);
	if (fd < 0) return 1;
	if (write(fd, cpix, sizeof(cpix)-1) != sizeof(cpix)-1) {
		close(fd);
		unlink(path);
		return 2;
	}
	close(fd);
	snprintf(args, sizeof(args), "cecrypt:cfile=%s", path);
	session = gf_fs_new_defaults(0);
	if (!session) {
		unlink(path);
		return 3;
	}
	filter = gf_fs_load_filter(session, args, &err);
	if (!filter || err != GF_OK) {
		gf_fs_del(session);
		unlink(path);
		return 4;
	}
	gf_fs_del(session);
	unlink(path);
	return 0;
}
