#!/bin/sh
# Build and run the residual reproducer against a built libarchive.
#
# Usage: sh pocs/run.sh [BUILD_DIR]
# BUILD_DIR (or $LIBARCHIVE_BUILD_DIR) must contain a libarchive built
# from this tree (static or shared), e.g. an AddressSanitizer build.
set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)

build_dir=${1:-${LIBARCHIVE_BUILD_DIR:-}}
if [ -z "$build_dir" ]; then
	for cand in "$root"/build*/libarchive "$root"/../build*/libarchive; do
		if [ -f "$cand/libarchive.a" ] || [ -f "$cand/libarchive.so" ]; then
			build_dir=${cand%/libarchive}
			break
		fi
	done
fi
if [ -z "$build_dir" ]; then
	echo "no built libarchive found; pass BUILD_DIR" >&2
	exit 2
fi

lib="$build_dir/libarchive"
if [ -f "$lib/libarchive.a" ]; then
	link="$lib/libarchive.a"
elif [ -f "$lib/libarchive.so" ]; then
	link="-L$lib -larchive -Wl,-rpath,$lib"
else
	echo "no libarchive archive found under $lib" >&2
	exit 2
fi

cc=${CC:-cc}
"$cc" -fsanitize=address -fno-omit-frame-pointer -g \
	-I"$root/libarchive" \
	"$here/residual.c" $link -o "$here/residual" ${LDADD:-}

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"
"$here/residual"
