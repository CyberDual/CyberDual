#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${EXR_BUILD:-}"
if [[ -z "$BUILD" ]]; then
  for d in /tmp/exr-asan "$ROOT/build" "$ROOT/build-asan" /tmp/exr-patched; do
    if [[ -f "$d/src/lib/OpenEXR/libOpenEXR-4_0.so" || -f "$d/src/lib/OpenEXR/libOpenEXR.so" ]]; then
      BUILD="$d"
      break
    fi
  done
fi
if [[ -z "$BUILD" ]]; then
  echo "set EXR_BUILD to an AddressSanitizer build of this tree" >&2
  exit 2
fi
INC=(
  -I"$ROOT/src/lib/OpenEXR"
  -I"$ROOT/src/lib/Iex"
  -I"$ROOT/src/lib/IlmThread"
  -I"$ROOT/src/lib/OpenEXRCore"
  -I"$BUILD/cmake"
  -isystem /usr/include/Imath
)
LIBS=(
  -L"$BUILD/src/lib/OpenEXR"
  -L"$BUILD/src/lib/OpenEXRCore"
  -L"$BUILD/src/lib/Iex"
  -L"$BUILD/src/lib/IlmThread"
  -lOpenEXR-4_0 -lOpenEXRCore-4_0 -lIex-4_0 -lIlmThread-4_0
  -Wl,-rpath,"$BUILD/src/lib/OpenEXR:$BUILD/src/lib/OpenEXRCore:$BUILD/src/lib/Iex:$BUILD/src/lib/IlmThread"
  -lImath-3_1
)
OUT="${TMPDIR:-/tmp}/lj2k_wide_row_poc"
c++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g "${INC[@]}" \
  "$ROOT/pocs/lj2k_wide_row.cpp" -o "$OUT" "${LIBS[@]}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:halt_on_error=1}"
exec "$OUT"
