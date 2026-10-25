#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build=""
if [ -n "${EXIV2_BUILD:-}" ] && [ -f "$EXIV2_BUILD/exiv2lib_export.h" ]; then
  build=$EXIV2_BUILD
fi
if [ -z "$build" ]; then
  for d in "$root"/build "$root"/build-* "$root"/asan_build* "$root"/linux_build* /tmp/exiv2-parent; do
    if [ -f "$d/exiv2lib_export.h" ] && [ -f "$d/lib/libexiv2.so" ]; then
      build=$d
      break
    fi
  done
fi
if [ -z "$build" ]; then
  echo "libexiv2 build not found" >&2
  exit 2
fi
out=${TMPDIR:-/tmp}/exiv2-tzpoc
c++ -std=c++20 -fsanitize=address -fno-omit-frame-pointer -g \
  -I"$root/include" -I"$build" \
  "$root/pocs/newbug.cpp" -L"$build/lib" -lexiv2 -o "$out"
ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=1} \
  LD_LIBRARY_PATH="$build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  exec "$out"
