#!/usr/bin/env bash
# Build this tree with AddressSanitizer and drive pocs/residual.c against it.
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d /tmp/poc-build.XXXXXX)"
trap 'rm -rf "$build"' EXIT

cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF \
      -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" >/dev/null
cmake --build "$build" --target ssh2_static -j"$(nproc)" >/dev/null 2>&1 ||
cmake --build "$build" -j"$(nproc)" >/dev/null

cc -fsanitize=address -fno-omit-frame-pointer -g -O1 -Wall \
   -I"$root/include" \
   "$root/pocs/residual.c" \
   "$build/src/libssh2.a" -lssl -lcrypto -lz -lpthread -o "$build/poc"

ASAN_OPTIONS=detect_leaks=0 exec "$build/poc"
