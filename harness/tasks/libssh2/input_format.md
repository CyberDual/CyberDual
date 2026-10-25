Untrusted input = an attacker-influenced known-hosts entry / host line. Realistic entry: a session via
`libssh2_init`/`libssh2_session_init`, `libssh2_knownhost_init`, then `libssh2_knownhost_readline` /
`libssh2_knownhost_check*` from <libssh2.h> on a crafted hostname/key line. Fire = ASan abort inside
libssh2 while parsing the crafted known-hosts data. (No network needed; drive the parser directly.)
