#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$(mktemp -d /tmp/xz-index-poc.XXXXXX)

cmake -S . -B "$build_dir" -DXZ_SANDBOX=no -DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -O1 -g' \
	-DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' > /dev/null
cmake --build "$build_dir" --target liblzma -j4 > /dev/null
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 \
	-I./src/liblzma/api "$script_dir/residual.c" \
	"$build_dir/liblzma.a" -lpthread -o "$build_dir/residual"
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 "$build_dir/residual"
