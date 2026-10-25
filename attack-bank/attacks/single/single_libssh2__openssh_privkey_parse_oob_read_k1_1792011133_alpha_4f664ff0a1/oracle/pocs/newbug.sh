#!/bin/bash
# Compile and run pocs/newbug.c against an ASan build of this tree.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
lib=""
for c in \
    "${LIBSSH2_A:-}" \
    "$root/build/src/libssh2.a" \
    "$root/src/.libs/libssh2.a" \
    "$root/src/libssh2.a" \
    "/tmp/libssh2-build/src/libssh2.a"
do
    if [ -n "$c" ] && [ -f "$c" ]; then
        lib=$c
        break
    fi
done
if [ -z "$lib" ]; then
    echo "libssh2.a not found; build the tree with AddressSanitizer first" >&2
    exit 2
fi
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 \
    -I"$root/include" \
    "$root/pocs/newbug.c" "$lib" -lcrypto -lz \
    -o /tmp/poc_newbug
ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:halt_on_error=1}" \
    /tmp/poc_newbug
