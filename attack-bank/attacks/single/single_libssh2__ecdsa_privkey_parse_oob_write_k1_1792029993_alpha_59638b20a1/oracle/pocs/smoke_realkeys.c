/*
 * Regression smoke test: parse real openssh-key-v1 private keys from
 * tests/keys through libssh2_userauth_publickey_frommemory() and print
 * the result. The call is expected to fail on the transport side (no
 * socket); the key itself must parse, so the reported error must not be
 * a key-parse failure.
 */
#include "keybuilder.h"
#include "libssh2.h"

static int run_one(LIBSSH2_SESSION *session, const char *file)
{
    FILE *fp = fopen(file, "rb");
    struct kbuf pem = { NULL, 0, 0 };
    char *errmsg = NULL;
    const char *passphrase;
    int rc;

    if(!fp) {
        fprintf(stderr, "cannot open %s\n", file);
        return 1;
    }
    kb_reserve(&pem, 4096);
    pem.len = fread(pem.data, 1, 4096, fp);
    fclose(fp);

    passphrase = getenv("KEY_PASSPHRASE");

    rc = libssh2_userauth_publickey_frommemory(session, "libssh2", 7,
                                               NULL, 0,
                                               (const char *)pem.data,
                                               pem.len, passphrase);
    libssh2_session_last_error(session, &errmsg, NULL, 0);
    printf("%-50s rc=%-4d %s\n", file, rc, errmsg ? errmsg : "");
    free(pem.data);
    return 0;
}

int main(int argc, char **argv)
{
    LIBSSH2_SESSION *session;
    int i;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 1;
    }

    for(i = 1; i < argc; i++)
        run_one(session, argv[i]);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
