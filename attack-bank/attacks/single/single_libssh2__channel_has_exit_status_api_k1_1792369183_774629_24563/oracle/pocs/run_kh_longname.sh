#!/bin/sh
# Build (if needed) and run the kh_longname PoC against the
# AddressSanitizer-instrumented static libssh2 library.
#
# Exit status mirrors the PoC itself: 0 on a clean run, 134 when the
# sanitizer aborts the process.
set -u

cd "$(dirname "$0")/.." || exit 2

b=.poc-asan
if [ ! -f "$b/src/libssh2.a" ]; then
    cmake -S . -B "$b" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -g" \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
        && cmake --build "$b" -j || exit 2
fi

cc -fsanitize=address -g -Iinclude pocs/kh_longname.c "$b/src/libssh2.a" \
    -lcrypto -o "$b/kh_longname" || exit 2

# Under some ASLR layouts the ASan runtime itself crashes before main() is
# reached in this environment (process dies with 139 and no report). Retry
# those startup failures so the returned status is the genuine outcome of
# the program: 0 (clean) or 134 (sanitizer abort).
i=0
rc=2
while [ "$i" -lt 12 ]; do
    i=$((i + 1))
    ASAN_OPTIONS=abort_on_error=1 "$b/kh_longname" >/dev/null 2>&1
    rc=$?
    if [ "$rc" -eq 0 ] || [ "$rc" -eq 134 ]; then
        break
    fi
done
exit "$rc"
