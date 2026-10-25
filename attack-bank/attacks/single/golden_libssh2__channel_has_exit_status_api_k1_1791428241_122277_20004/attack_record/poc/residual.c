/* PoC: libssh2_channel_has_exit_status() dereferences a NULL channel.
 *
 * libssh2_channel_get_exit_status() and libssh2_channel_get_exit_signal()
 * both document and implement "return a zero if channel is NULL" so a
 * caller may safely query a channel handle after a failed
 * libssh2_channel_open_session()/libssh2_channel_open_ex() without an
 * extra NULL check. libssh2_channel_has_exit_status() is documented the
 * same way but is missing the guard, so passing it the documented NULL
 * value crashes instead of returning 0.
 */

#include <libssh2.h>
#include <stdio.h>

int main(void)
{
    int rc;

    rc = libssh2_init(0);
    if(rc) {
        fprintf(stderr, "libssh2_init failed (%d)\n", rc);
        return 1;
    }

    /* Documented, in-contract usage: a NULL channel (e.g. because
       libssh2_channel_open_session() failed) must be accepted and
       reported as "no exit status", per the function's own doc comment. */
    rc = libssh2_channel_has_exit_status(NULL);

    fprintf(stderr, "libssh2_channel_has_exit_status(NULL) returned %d\n", rc);

    libssh2_exit();
    return 0;
}
