#define _POSIX_C_SOURCE 200809L
#include <gpac/filters.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    static const char cpix[] =
        "<?xml version=\"1.0\"?>"
        "<cpix:CPIX xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
        "<cpix:ContentKeyList><cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" "
        "explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
        "<cpix:Data><pskc:Secret><pskc:PlainValue>\nFzsa6fC/yLr6IPmeug0H2Q==\n</pskc:PlainValue>"
        "</pskc:Secret></cpix:Data></cpix:ContentKey></cpix:ContentKeyList></cpix:CPIX>";
    char path[] = "/tmp/gpac-cpix-XXXXXX";
    char args[128];
    GF_FilterSession *session;
    GF_Filter *filter;
    GF_Err err = GF_OK;
    int fd, ok;

    fd = mkstemp(path);
    if (fd < 0) return 1;
    if (write(fd, cpix, sizeof(cpix) - 1) != (ssize_t) (sizeof(cpix) - 1)) {
        close(fd);
        unlink(path);
        return 1;
    }
    close(fd);
    session = gf_fs_new_defaults(0);
    if (!session) {
        unlink(path);
        return 1;
    }
    snprintf(args, sizeof(args), "cecrypt:cfile=%s", path);
    filter = gf_fs_load_filter(session, args, &err);
    ok = filter != NULL && err == GF_OK;
    gf_fs_del(session);
    unlink(path);
    return ok ? 0 : 2;
}
