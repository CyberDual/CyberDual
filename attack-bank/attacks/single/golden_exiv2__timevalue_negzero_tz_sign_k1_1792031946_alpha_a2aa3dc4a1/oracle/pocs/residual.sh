#!/bin/bash
# Build and run the TimeValue negative-zero basic-offset PoC against a
# sanitizer build of libexiv2.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
lib=""
for cand in \
  "${EXIV2_LIB:-}" \
  "$root"/build*/lib/libexiv2.so \
  "$root"/build/lib/libexiv2.so \
  /tmp/exiv2-asan/lib/libexiv2.so
do
  if [ -n "$cand" ] && [ -e "$cand" ]; then
    lib="$cand"
    break
  fi
done
if [ -z "$lib" ]; then
  lib="$(find "$root" /tmp -name 'libexiv2.so' -print -quit 2>/dev/null || true)"
fi
if [ -z "$lib" ]; then
  echo "libexiv2.so not found" >&2
  exit 2
fi
libdir="$(dirname "$lib")"
bdir="$(dirname "$libdir")"
incs=(-I"$root/include")
if [ -f "$bdir/exv_conf.h" ]; then
  incs+=(-I"$bdir")
fi
out="${TMPDIR:-/tmp}/timevalue_residual_poc"
c++ -std=c++20 -fsanitize=address,undefined -fno-omit-frame-pointer -g -O0 \
  "${incs[@]}" "$root/pocs/residual.cpp" -L"$libdir" -lexiv2 -o "$out"
LD_LIBRARY_PATH="$libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$out"
