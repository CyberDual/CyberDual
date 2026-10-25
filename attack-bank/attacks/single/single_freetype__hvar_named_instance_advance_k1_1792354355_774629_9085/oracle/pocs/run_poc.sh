#!/bin/sh
# Build the PoC against an AddressSanitizer build of the freetype tree
# and run it.  Safe to run repeatedly; the library build is cached.
set -e

cd "$(dirname "$0")/.."
ROOT=$(pwd)
BLD="$ROOT/build-asan-poc"

if [ ! -f "$BLD/libfreetype.a" ]; then
  CC=gcc CFLAGS="-fsanitize=address -g -O1" meson setup "$ROOT" "$BLD" \
    --default-library=static \
    -Dzlib=disabled -Dbrotli=disabled -Dbzip2=disabled \
    -Dpng=disabled -Dharfbuzz=disabled -Dtests=disabled > /dev/null
  ninja -C "$BLD" > /dev/null
fi

gcc -fsanitize=address -no-pie -g -O1 \
  -I "$ROOT/include" -I "$ROOT/pocs" \
  "$ROOT/pocs/hvar_named_instance.c" \
  "$BLD/libfreetype.a" \
  -o "$BLD/hvar_named_instance"

ASAN_OPTIONS=abort_on_error=1 "$BLD/hvar_named_instance"
