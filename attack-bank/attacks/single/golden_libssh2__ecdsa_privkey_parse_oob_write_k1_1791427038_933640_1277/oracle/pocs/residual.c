#include <libssh2.h>
#include <stdio.h>
#include <string.h>

static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAiAAAABNlY2RzYS\n"
    "1zaGEyLW5pc3RwMzg0AAAACG5pc3RwMzg0AAAAYQTosiScH/oRSazpIpPSEFcY4YVZyNby\n"
    "peARi49N3qy78OE118KGc5T8eifd+n1PSb7z8PnfDwOL4jBHxW5nWx0RCocIt7tb2a349J\n"
    "gfEl8PegHGcF/DwC+eesIKJvv0MfkAAADIKLgw6yi4MOsAAAATZWNkc2Etc2hhMi1uaXN0\n"
    "cDM4NAAAAAhuaXN0cDM4NAAAAGEE6LIknB/6EUms6SKT0hBXGOGFWcjW8qXgEYuPTd6su/\n"
    "DhNdfChnOU/Hon3fp9T0m+8/D53w8Di+IwR8VuZ1sdEQqHCLe7W9mt+PSYHxJfD3oBxnBf\n"
    "w8AvnnrCCib79DH5AAAAMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
    "AAAAAAAAAAAAAAAAAAAAA=\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0))
        return 2;
    session = libssh2_session_init();
    if(!session)
        return 2;
    rc = libssh2_userauth_publickey_frommemory(session,
                                                "user", 4, NULL, 0,
                                                key, strlen(key), NULL);
    printf("return code: %d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
