#!/bin/sh
set -eu

src_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(mktemp -d /tmp/libssh2-knownhost.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT

cmake -S "$src_dir" -B "$build_dir/build" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -O1 -g' \
    -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address' \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DBUILD_STATIC_LIBS=OFF -DBUILD_SHARED_LIBS=ON
cmake --build "$build_dir/build" --target libssh2_shared -j 4
cc -fsanitize=address -fno-omit-frame-pointer -g \
    -I"$src_dir/include" "$src_dir/pocs/knownhost_salt.c" \
    -L"$build_dir/build/src" \
    -Wl,-rpath,"$build_dir/build/src" -lssh2 \
    -o "$build_dir/knownhost_salt"
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 "$build_dir/knownhost_salt"
