#!/bin/sh
# Builds this PoC against the liblzma sources in the tree that contains
# this script and runs the resulting binary under AddressSanitizer.
#
# Usage: sh pocs/run.sh
set -eu

poc_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
src_dir=$(CDPATH= cd -- "$poc_dir/.." && pwd)

build_dir=${TMPDIR:-/tmp}/xz-index-poc-build-$$
trap 'rm -rf "$build_dir"' EXIT

cmake -S "$src_dir" -B "$build_dir" \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -O1 -g' \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_TESTING=OFF \
		-DXZ_SANDBOX=no > /dev/null

cmake --build "$build_dir" --target liblzma > /dev/null

cc -fsanitize=address -fno-omit-frame-pointer -O1 -g \
		-I "$src_dir/src/liblzma/api" \
		"$poc_dir/residual.c" \
		"$build_dir/liblzma.a" \
		-o "$build_dir/residual"

exec "$build_dir/residual"
