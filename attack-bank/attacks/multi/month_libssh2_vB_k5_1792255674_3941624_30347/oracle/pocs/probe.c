#include <libssh2.h>

#include <stdio.h>

int main(void)
{
    static const char line[] =
        "example.test ecdsa-sha2-nistp256-cert-v01@openssh.com "
        "AAAAKGVjZHNhLXNoYTItbmlzdHAyNTYtY2VydC12MDFAb3BlbnNzaC5jb20AAAAg"
        "B4UbNhvuwfCifhqOeI/bp/GT3s8xYofpyeavA+75l5sAAAAIbmlzdHAyNTYAAABB"
        "BC+/syyeKJD9dC2ZH9Q7iJGReR4YM3rUCMsSynkyXojdfSClGCMY7JvWlt30ESjY"
        "vxoTfSRGx6WvaqYK/vPoYQ4AAAAAAAAAAAAAAAIAAAAKaG9zdF9lY2RzYQAAAAs"
        "AAAAHbGlic3NoMgAAAAAAAAAA//////////8AAAAAAAAAAAAAAAAAAABoAAAAE2Vj"
        "ZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBCVlWpcb+zZzGy25G7ZU"
        "PVRj2OSXjiIJ7HS/X+4cCRlQrFWeu2QH5H16Zko5hG5vxIPV3O5yCDv4fXfghpxCy"
        "OkAAABjAAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABIAAAA"
        "IES/HuhK5z/HP553OzMXDIK8JU/yrgg5JdaClqA1jgT+AAAAIFnDIf0BdY03VNTQ"
        "5tjfQLeafUGDsCDYk/itn0kq28AP openssh_server/ssh_host_ecdsa_key.pub";
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        libssh2_exit();
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    rc = libssh2_knownhost_readline(hosts, line, sizeof(line) - 1,
                                     LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("knownhost_readline rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
