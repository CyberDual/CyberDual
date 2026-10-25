#!/bin/sh
# Compile and run the CRW writeMetadata reproducer against an ASan build.
set -eu
cd "$(dirname "$0")/.."

pick() {
  d=$1
  if [ -n "$d" ] && [ -e "$d/lib/libexiv2.so" ] && [ -f "$d/exv_conf.h" ]; then
    printf '%s\n' "$d"
    return 0
  fi
  return 1
}

build=""
if [ -n "${EXIV2_BUILD:-}" ]; then
  build=$(pick "$EXIV2_BUILD" || true)
fi
if [ -z "$build" ]; then
  for d in build build-linux-sanitizers asan_build /tmp/exiv2-asan; do
    build=$(pick "$d" || true)
    [ -n "$build" ] && break
  done
fi
if [ -z "$build" ]; then
  for conf in $(find . /tmp -name exv_conf.h -type f 2>/dev/null); do
    d=$(dirname "$conf")
    build=$(pick "$d" || true)
    [ -n "$build" ] && break
  done
fi
if [ -z "$build" ]; then
  echo "exiv2 build not found (set EXIV2_BUILD)" >&2
  exit 2
fi

out=${POC_OUT:-/tmp/poc_residual}
c++ -std=c++20 -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Iinclude -I"$build" pocs/residual.cpp \
  -L"$build/lib" -Wl,-rpath,"$build/lib" -lexiv2 -o "$out"
"$out"
