#!/bin/sh
# Build libssh2 (static, ASan-instrumented) from the current tree, build the
# PoC against it, and run it. On the vulnerable tree the PoC aborts under
# AddressSanitizer while the client processes the crafted GCM packet.
set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

bdir=$(mktemp -d)
trap 'rm -rf "$bdir"' EXIT

cmake -S . -B "$bdir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-fsanitize=address -g -O1 -fno-omit-frame-pointer" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF
cmake --build "$bdir" -j"$(nproc 2>/dev/null || echo 4)"

cc -fsanitize=address -g -O1 -fno-omit-frame-pointer -Iinclude \
    -o "$bdir/poc-residual" pocs/residual.c "$bdir/src/libssh2.a" \
    -lssl -lcrypto -lz

attempt=1
while [ "$attempt" -le 30 ]; do
    out=$(ASAN_OPTIONS=detect_leaks=0:handle_segv=0 "$bdir/poc-residual" 2>&1) \
        && rc=0 || rc=$?
    printf '%s\n' "$out"
    case "$out" in
        *"ERROR: AddressSanitizer"*)
            exit "$rc" ;;          # sanitizer abort demonstrated
    esac
    if [ "$rc" -eq 0 ]; then
        exit 0                     # ran to completion, no sanitizer report
    fi
    if [ "$rc" -ne 139 ]; then
        exit "$rc"                 # unexpected failure, surface it
    fi
    attempt=$((attempt + 1))       # startup crash without report: retry
done

echo "run.sh: no definitive result after 30 attempts" >&2
exit 2
